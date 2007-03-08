/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the RSS Parsing Engine
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the 
 * ***** END LICENSE BLOCK ***** */

// The feed parser depends on FeedItems.js, Feed.js.

var rdfcontainer =  Components.classes["@mozilla.org/rdf/container-utils;1"].getService(Components.interfaces.nsIRDFContainerUtils);
var rdfparser = Components.classes["@mozilla.org/rdf/xml-parser;1"].createInstance(Components.interfaces.nsIRDFXMLParser);
var serializer = Components.classes["@mozilla.org/xmlextras/xmlserializer;1"].createInstance(Components.interfaces.nsIDOMSerializer);

function FeedParser() 
{}

FeedParser.prototype = 
{
  // parseFeed returns an array of parsed items ready for processing
  // it is currently a synchronous operation. If there was an error parsing the feed, 
  // parseFeed returns an empty feed in addition to calling aFeed.onParseError
  parseFeed: function (aFeed, aSource, aDOM, aBaseURI)
  {
    if (!aSource || !(aDOM instanceof Components.interfaces.nsIDOMXMLDocument))
    {
      aFeed.onParseError(aFeed);   
      return new Array();
    }
    else if((aDOM.documentElement.namespaceURI == "http://www.w3.org/1999/02/22-rdf-syntax-ns#")
            && (aDOM.documentElement.getElementsByTagNameNS("http://purl.org/rss/1.0/", "channel")[0]))
    {
      debug(aFeed.url + " is an RSS 1.x (RDF-based) feed");
      // aSource can be misencoded (XMLHttpRequest converts to UTF-8 by default), 
      // but the DOM is almost always right because it uses the hints in the XML file.
      // This is slower, but not noticably so. Mozilla doesn't have the 
      // XMLHttpRequest.responseBody property that IE has, which provides access 
      // to the unencoded response.
      var xmlString=serializer.serializeToString(aDOM.documentElement);
      return this.parseAsRSS1(aFeed, xmlString, aBaseURI);
    }
    else if (aDOM.documentElement.namespaceURI == ATOM_03_NS)
    {
      debug(aFeed.url + " is an Atom 0.3 feed");
      return this.parseAsAtom(aFeed, aDOM);
    }
    else if (aDOM.documentElement.namespaceURI == ATOM_IETF_NS)
    {
      debug(aFeed.url + " is an IETF Atom feed");
      return this.parseAsAtomIETF(aFeed, aDOM);
    }
    else if (aSource.search(/"http:\/\/my\.netscape\.com\/rdf\/simple\/0\.9\/"/) != -1)
    {
      debug(aFeed.url + " is an 0.90 feed");
      return this.parseAsRSS2(aFeed, aDOM);
    }
    // XXX Explicitly check for RSS 2.0 instead of letting it be handled by the
    // default behavior (who knows, we may change the default at some point).
    else 
    {
      // We don't know what kind of feed this is; let's pretend it's RSS 0.9x
      // and hope things work out for the best.  In theory even RSS 1.0 feeds
      // could be parsed by the 0.9x parser if the RSS namespace was the default.
      debug(aFeed.url + " is of unknown format; assuming an RSS 0.9x feed");
      return this.parseAsRSS2(aFeed, aDOM);
    }
  },

  parseAsRSS2: function (aFeed, aDOM) 
  {
    // Get the first channel (assuming there is only one per RSS File).
    var parsedItems = new Array();

    var channel = aDOM.getElementsByTagName("channel")[0];
    if (!channel)
      return aFeed.onParseError(aFeed);

    //usually the empty string, unless this is RSS .90
    var nsURI = channel.namespaceURI || "";
    debug("channel NS: '" + nsURI +"'");

    aFeed.title = aFeed.title || getNodeValue(this.childrenByTagNameNS(channel, nsURI, "title")[0]);
    aFeed.description = getNodeValue(this.childrenByTagNameNS(channel, nsURI, "description")[0]);
    aFeed.link = getNodeValue(this.childrenByTagNameNS(channel, nsURI, "link")[0]);

    if (!aFeed.parseItems)
      return parsedItems;

    aFeed.invalidateItems();
    // XXX use getElementsByTagNameNS for now
    // childrenByTagNameNS would be better, but RSS .90 is still with us
    var itemNodes = aDOM.getElementsByTagNameNS(nsURI,"item");

    for (var i=0; i < itemNodes.length; i++) 
    {
      var itemNode = itemNodes[i];
      var item = new FeedItem();
      item.feed = aFeed;
      item.characterSet = "UTF-8";

      var link = getNodeValue(this.childrenByTagNameNS(itemNode, nsURI, "link")[0]);
      var guidNode = this.childrenByTagNameNS(itemNode, nsURI, "guid")[0];
      var guid;
      var isPermaLink;
      if (guidNode) 
      {
        guid = getNodeValue(guidNode);
        isPermaLink = guidNode.getAttribute('isPermaLink') == 'false' ? false : true;
      }

      item.isStoredWithId = true;
      item.url = link ? link : (guid && isPermaLink) ? guid : null;
      item.id = guid;
      item.description = getNodeValue(this.childrenByTagNameNS(itemNode, nsURI, "description")[0]);
      item.title = getNodeValue(this.childrenByTagNameNS(itemNode, nsURI, "title")[0])
                   || (item.description ? (this.stripTags(item.description).substr(0, 150)) : null)
                   || item.title;

      item.author = getNodeValue(this.childrenByTagNameNS(itemNode, nsURI, "author")[0]
                                 || this.childrenByTagNameNS(itemNode, DC_NS, "creator")[0])
                                 || aFeed.title
                                 || item.author;
      item.date = getNodeValue(this.childrenByTagNameNS(itemNode, nsURI, "pubDate")[0]
                               || this.childrenByTagNameNS(itemNode, DC_NS, "date")[0])
                               || item.date;
    
      // If the date is invalid, users will see the beginning of the epoch
      // unless we reset it here, so they'll see the current time instead.
      // This is typical aggregator behavior.
      if(item.date)
      {
        item.date = trimString(item.date);
        if(!isValidRFC822Date(item.date))
        {
          // XXX Use this on the other formats as well
          item.date = dateRescue(item.date);
        }
      }

      var content = getNodeValue(this.childrenByTagNameNS(itemNode, RSS_CONTENT_NS, "encoded")[0]);
      if(content)
        item.content = content;

      // Handle an enclosure (if present)
      var enclosureNode = this.childrenByTagNameNS(itemNode, nsURI, "enclosure")[0];
      if (enclosureNode)
        item.enclosure = new FeedEnclosure(enclosureNode.getAttribute("url"), 
                                          enclosureNode.getAttribute("type"),
                                          enclosureNode.getAttribute("length"));
      parsedItems[i] = item;
    }
    return parsedItems;
  },

  parseAsRSS1 : function(aFeed, aSource, aBaseURI) 
  {
    var parsedItems = new Array();

    // RSS 1.0 is valid RDF, so use the RDF parser/service to extract data.
    // Create a new RDF data source and parse the feed into it.
    var ds = Components.classes["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"]
             .createInstance(Components.interfaces.nsIRDFDataSource);

    rdfparser.parseString(ds, aBaseURI, aSource);
    
    // Get information about the feed as a whole.
    var channel = ds.GetSource(RDF_TYPE, RSS_CHANNEL, true);
    
    aFeed.title = aFeed.title || getRDFTargetValue(ds, channel, RSS_TITLE) || aFeed.url;
    aFeed.description = getRDFTargetValue(ds, channel, RSS_DESCRIPTION) || "";
    aFeed.link = getRDFTargetValue(ds, channel, RSS_LINK) || aFeed.url;

    if (!aFeed.parseItems)
      return parsedItems;

    aFeed.invalidateItems();

    var items = ds.GetTarget(channel, RSS_ITEMS, true);
    if (items)
      items = rdfcontainer.MakeSeq(ds, items).GetElements();
  
    // If the channel doesn't list any items, look for resources of type "item"
    // (a hacky workaround for some buggy feeds).
    if (!items || !items.hasMoreElements())
      items = ds.GetSources(RDF_TYPE, RSS_ITEM, true);

    var index = 0; 
    while (items.hasMoreElements()) 
    {
      var itemResource = items.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
      var item = new FeedItem();
      item.feed = aFeed;
      item.characterSet = "UTF-8";

      // Prefer the value of the link tag to the item URI since the URI could be
      // a relative URN.
      var uri = itemResource.Value;
      var link = getRDFTargetValue(ds, itemResource, RSS_LINK);

      // XXX
      // check for bug258465 -- entities appear escaped 
      // in the value returned by getRDFTargetValue when they shouldn't
      //debug("link comparison\n" + " uri: " + uri + "\nlink: " + link);

      item.url = link || uri;
      item.id = item.url;
      item.description = getRDFTargetValue(ds, itemResource, RSS_DESCRIPTION);
      item.title = getRDFTargetValue(ds, itemResource, RSS_TITLE)
                                     || getRDFTargetValue(ds, itemResource, DC_SUBJECT)
                                     || (item.description ? (this.stripTags(item.description).substr(0, 150)) : null)
                                     || item.title;
      item.author = getRDFTargetValue(ds, itemResource, DC_CREATOR)
                                      || getRDFTargetValue(ds, channel, DC_CREATOR)
                                      || aFeed.title
                                      || item.author;
      
      item.date = getRDFTargetValue(ds, itemResource, DC_DATE) || item.date;
      item.content = getRDFTargetValue(ds, itemResource, RSS_CONTENT_ENCODED);

      parsedItems[index++] = item;
    }
  
    return parsedItems;
  },

  parseAsAtom: function(aFeed, aDOM) 
  {
    var parsedItems = new Array();

    // Get the first channel (assuming there is only one per Atom File).
    var channel = aDOM.getElementsByTagName("feed")[0];
    if (!channel)
    {
      aFeed.onParseError(aFeed);
      return parsedItems;
    }

    aFeed.title = aFeed.title || this.stripTags(getNodeValue(this.childrenByTagNameNS(channel, ATOM_03_NS, "title")[0]));
    aFeed.description = getNodeValue(this.childrenByTagNameNS(channel, ATOM_03_NS, "tagline")[0]);
    aFeed.link = this.findAtomLink("alternate",this.childrenByTagNameNS(channel, ATOM_03_NS, "link"));

    if (!aFeed.parseItems)
      return parsedItems;

    aFeed.invalidateItems();
    var items = this.childrenByTagNameNS(channel, ATOM_03_NS, "entry");
    debug("Items to parse: " + items.length);
  
    for (var i=0; i < items.length; i++) 
    {
      var itemNode = items[i];
      var item = new FeedItem();
      item.feed = aFeed;
      item.characterSet = "UTF-8";

      var url;
      url = this.findAtomLink("alternate",this.childrenByTagNameNS(itemNode, ATOM_03_NS, "link"));

      item.url = url;
      item.id = getNodeValue(this.childrenByTagNameNS(itemNode, ATOM_03_NS, "id")[0]);
      item.description = getNodeValue(this.childrenByTagNameNS(itemNode, ATOM_03_NS, "summary")[0]);
      item.title = getNodeValue(this.childrenByTagNameNS(itemNode, ATOM_03_NS, "title")[0])
                                || (item.description ? item.description.substr(0, 150) : null)
                                || item.title;

      var authorEl = this.childrenByTagNameNS(itemNode, ATOM_03_NS, "author")[0]
                     || this.childrenByTagNameNS(itemNode, ATOM_03_NS, "contributor")[0]
                     || this.childrenByTagNameNS(channel, ATOM_03_NS, "author")[0];
      var author = "";

      if (authorEl) 
      {
        var name = getNodeValue(this.childrenByTagNameNS(authorEl, ATOM_03_NS, "name")[0]);
        var email = getNodeValue(this.childrenByTagNameNS(authorEl, ATOM_03_NS, "email")[0]);
        if (name)
          author = name + (email ? " <" + email + ">" : "");
        else if (email)
          author = email;
      }
      
      item.author = author || item.author || aFeed.title;

      item.date = getNodeValue(this.childrenByTagNameNS(itemNode, ATOM_03_NS, "modified")[0]
                               || this.childrenByTagNameNS(itemNode, ATOM_03_NS, "issued")[0]
                               || this.childrenByTagNameNS(itemNode, ATOM_03_NS, "created")[0])
                               || item.date;

      // XXX We should get the xml:base attribute from the content tag as well
      // and use it as the base HREF of the message.
      // XXX Atom feeds can have multiple content elements; we should differentiate
      // between them and pick the best one.
      // Some Atom feeds wrap the content in a CTYPE declaration; others use
      // a namespace to identify the tags as HTML; and a few are buggy and put
      // HTML tags in without declaring their namespace so they look like Atom.
      // We deal with the first two but not the third.
      
      var content;
      var contentNode = this.childrenByTagNameNS(itemNode, ATOM_03_NS, "content")[0];
      if (contentNode) 
      {
        content = "";
        for (var j=0; j < contentNode.childNodes.length; j++) 
        {
          var node = contentNode.childNodes.item(j);
          if (node.nodeType == node.CDATA_SECTION_NODE)
            content += node.data;
          else
            content += serializer.serializeToString(node);
        }
      
        if (contentNode.getAttribute('mode') == "escaped") 
        {
          content = content.replace(/&lt;/g, "<");
          content = content.replace(/&gt;/g, ">");
          content = content.replace(/&amp;/g, "&");
        }
      
        if (content == "")
          content = null;
      }
      
      item.content = content;
      parsedItems[i] = item;
    }
    return parsedItems;
  },

  parseAsAtomIETF: function(aFeed, aDOM)
  {
    
    var parsedItems = new Array();

    // Get the first channel (assuming there is only one per Atom File).
    var channel = this.childrenByTagNameNS(aDOM,ATOM_IETF_NS,"feed")[0];
    if (!channel)
    {
      aFeed.onParseError(aFeed);
      return parsedItems;
    }

    aFeed.title = aFeed.title || this.stripTags(this.serializeTextConstruct(this.childrenByTagNameNS(channel,ATOM_IETF_NS,"title")[0]));
    aFeed.description = this.serializeTextConstruct(this.childrenByTagNameNS(channel,ATOM_IETF_NS,"subtitle")[0]);
    aFeed.link = this.findAtomLink("alternate", this.childrenByTagNameNS(channel,ATOM_IETF_NS,"link"));
    
    if (!aFeed.parseItems)
      return parsedItems;

    aFeed.invalidateItems();
    var items = this.childrenByTagNameNS(channel,ATOM_IETF_NS,"entry");
    debug("Items to parse: " + items.length);

    for (var i=0; i < items.length; i++) 
    {
      var itemNode = items[i];
      var item = new FeedItem();
      item.feed = aFeed;
      item.characterSet = "UTF-8";
      item.isStoredWithId = true;
      item.url = this.findAtomLink("alternate", this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "link")) || aFeed.link;
      item.id = getNodeValue(this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "id")[0]);
      item.description = this.serializeTextConstruct(this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "summary")[0]);
      item.title = this.stripTags(this.serializeTextConstruct(this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "title")[0])
                                  || (item.description ? item.description.substr(0, 150) : null)
                                  || item.title);
      
      // XXX Support multiple authors
      var source = this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "source")[0];
      var authorEl = this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "author")[0]
                           || (source ? this.childrenByTagNameNS(source, ATOM_IETF_NS, "author")[0] : null)
                           || this.childrenByTagNameNS(channel, ATOM_IETF_NS, "author")[0];
      var author = "";

      if (authorEl) 
      {
        var name = getNodeValue(this.childrenByTagNameNS(authorEl, ATOM_IETF_NS, "name")[0]);
        var email = getNodeValue(this.childrenByTagNameNS(authorEl, ATOM_IETF_NS, "email")[0]);
        if (name)
          author = name + (email ? " <" + email + ">" : "");
        else if (email)
          author = email;
      }
      
      item.author = author || item.author || aFeed.title;
      item.date = getNodeValue(this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "updated")[0]
                               || this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "published")[0])
                               || item.date;

      item.content = this.serializeTextConstruct(this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "content")[0]);

      if(item.content)
        item.xmlContentBase = this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "content")[0].baseURI;
      else if(item.description)
        item.xmlContentBase = this.childrenByTagNameNS(itemNode, ATOM_IETF_NS, "summary")[0].baseURI;
      else
        item.xmlContentBase = itemNode.baseURI;

      parsedItems[i] = item;
    }

    return parsedItems;

  },

  serializeTextConstruct: function(textElement)
  {
    var content = "";

    if (textElement) 
    {
      var textType = textElement.getAttribute('type');

      // Atom spec says consider it "text" if not present
      if(!textType)
        textType = "text";

      // There could be some strange content type we don't handle
      if((textType != "text") && (textType != "html") && (textType != "xhtml"))
        return null;

      for (var j=0; j < textElement.childNodes.length; j++) 
      {
        var node = textElement.childNodes.item(j);
        if (node.nodeType == node.CDATA_SECTION_NODE)
          content += this.xmlEscape(node.data);
        else
          content += serializer.serializeToString(node);
      }
      if (textType == "html") 
        content = this.xmlUnescape(content);
    }

    // other parts of the code depend on this being null
    // if there's no content
    return content ? content : null;
  },

  // finds elements that are direct children of the first arg
  childrenByTagNameNS: function(aElement, aNamespace, aTagName)
  {
    var matches = aElement.getElementsByTagNameNS(aNamespace, aTagName);
    var matchingChildren = new Array();
    for (var i = 0; i < matches.length; i++) 
    {
      if(matches[i].parentNode == aElement)
        matchingChildren.push(matches[i])
    }
    return matchingChildren;
  },

  findAtomLink: function(linkRel, linkElements)
  {
    // XXX Need to check for MIME type and hreflang
    for ( var j=0 ; j<linkElements.length ; j++ ) {
      var alink = linkElements[j];
      if (alink && 
          //if there's a link rel
          ((alink.getAttribute('rel') && alink.getAttribute('rel') == linkRel) ||
           //if there isn't, assume 'alternate'
           (!alink.getAttribute('rel') && (linkRel=="alternate"))) 
          && alink.getAttribute('href')) 
      {
        // Atom links are interpreted relative to xml:base
        var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                   .getService(Components.interfaces.nsIIOService);
        url = ioService.newURI(alink.baseURI, null, null);
        return url.resolve(alink.getAttribute('href'));
      }
    }
    return null;
  },
  
  stripTags: function(someHTML)
  {
    return someHTML ? someHTML.replace(/<[^>]+>/g,"") : someHTML;
  },

  xmlUnescape: function(s)
  {
    s = s.replace(/&lt;/g, "<");
    s = s.replace(/&gt;/g, ">");
    s = s.replace(/&amp;/g, "&");
    return s;
  },

  xmlEscape: function(s)
  {
    s = s.replace(/&/g, "&amp;");
    s = s.replace(/>/g, "&gt;");
    s = s.replace(/</g, "&lt;");
    return s;
   }
};

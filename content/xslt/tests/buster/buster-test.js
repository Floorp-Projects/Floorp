/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var parser = new DOMParser();
var methodExpr = (new XPathEvaluator).createExpression("xsl:output/@method",
    {
    lookupNamespaceURI: function(aPrefix)
        {
            if (aPrefix == "xsl")
                return "http://www.w3.org/1999/XSL/Transform";
            return "";
        }
    });

const nsIWebProgListener = Components.interfaces.nsIWebProgressListener;

var runQueue = 
{
    mArray : new Array(),
    push : function(aRunItem)
    {
        this.mArray.push(aRunItem);
    },
    observe : function(aSubject, aTopic, aData)
    {
        var item = this.mArray.shift();
        if (item) {
            item.run(this);
        }
    },
    run : function()
    {
        this.observe(null,'','');
    }
}

var itemCache = 
{
    mArray : new Array(),
    getItem : function(aResource)
    {
        // Directory selected
        if (kContUtils.IsSeq(runItem.prototype.kDatabase, aResource)) {
            var aSeq = kContUtils.MakeSeq(runItem.prototype.kDatabase, aResource);
            dump("sequence: "+aSeq+" with "+aSeq.GetCount()+" elements\n");
            var child, children = aSeq.GetElements();
            var m = 0, first;
            while (children.hasMoreElements()) {
                m += 1;
                child = children.getNext();
                child.QueryInterface(nsIRDFResource);
                if (!first)
                    first = itemCache.getItem(child);
                else
                    itemCache.getItem(child);
            }
            return first;
        }
        if (aResource.Value in this.mArray) {
            return this.mArray[aResource.Value];
        }
        var retItem = new runItem(aResource);
        this.mArray[aResource.Value] = retItem;
        runQueue.push(retItem);
        return retItem;
    },
    rerunItem : function(aResource, aObserver)
    {
        var anItem = new runItem(aResource);
        this.mArray[aResource.Value] = anItem;
        anItem.run(aObserver);
    },
    observe : function(aSubject, aTopic, aData)
    {
        this.mRun += 1;
        if (aTopic == "success") {
            if (aData == "yes") {
                this.mGood += 1;
            }
            else {
                this.mFalse +=1;
            }
        }
    }
}

function runItem(aResource)
{
  this.mResource = aResource;
  // Directory selected
  if (kContUtils.IsSeq(this.kDatabase,this.mResource)) {
      var aSeq = kContUtils.MakeSeq(this.kDatabase,this.mResource);
      dump("THIS SHOULDN'T HAPPEN\n");
      var child, children = aSeq.GetElements();
      var m = 0;
      while (children.hasMoreElements()) {
          m += 1;
          child = children.getNext();
          child.QueryInterface(nsIRDFResource);
          itemCache.getItem(child);
      }
  }
}

runItem.prototype = 
{
    // RDF resource associated with this test
    mResource : null,
    // XML documents for the XSLT transformation
    mSourceDoc : null,
    mStyleDoc  : null,
    mResDoc    : null,
    // XML or plaintext document for the reference
    mRefDoc    : null,
    // bitfield signaling the loaded documents
    mLoaded    : 0,
    kSource    : 1,
    kStyle     : 2,
    kReference : 4,
    // a observer, potential argument to run()
    mObserver  : null,
    mSuccess   : null,
    mMethod    : 'xml',
    // XSLTProcessor, shared by the instances
    kProcessor : new XSLTProcessor(),
    kXalan     : kStandardURL.createInstance(nsIURL),
    kDatabase  : null,
    kObservers : new Array(),

    run : function(aObserver)
    {
        if (aObserver && typeof(aObserver)=='function' ||
            (typeof(aObserver)=='object' && 
             typeof(aObserver.observe)=='function')) {
            this.mObserver=aObserver;
        }
        var name = this.kDatabase.GetTarget(this.mResource, krTypeName, true);
        if (name) {
            var cat = this.kDatabase.GetTarget(this.mResource, krTypeCat, true);
            var path = this.kDatabase.GetTarget(this.mResource, krTypePath, true);
            cat = cat.QueryInterface(nsIRDFLiteral);
            name = name.QueryInterface(nsIRDFLiteral);
            path = path.QueryInterface(nsIRDFLiteral);
            var xalan_fl  = this.kXalan.resolve(cat.Value+"/"+path.Value);
            var xalan_ref  = this.kXalan.resolve(cat.Value+"-gold/"+path.Value);
            this.mRefURL =
                this.kXalan.resolve(cat.Value + "-gold/" + path.Value + ".out");
            dump(name.Value+" links to "+xalan_fl+"\n");
        }
        // Directory selected
        if (kContUtils.IsSeq(this.kDatabase,this.mResource)) {
            return;
            var aSeq = kContUtils.MakeSeq(this.kDatabase,this.mResource);
            dump("sequence: "+aSeq+" with "+aSeq.GetCount()+" elements\n");
            var child, children = aSeq.GetElements();
            var m = 0;
            while (children.hasMoreElements()) {
                m += 1;
                child = children.getNext();
                child.QueryInterface(nsIRDFResource);
            }
        }
        this.mSourceDoc = document.implementation.createDocument('', '', null);
        this.mSourceDoc.addEventListener("load",this.onload(1),false);
        this.mSourceDoc.load(xalan_fl+".xml");
        this.mStyleDoc = document.implementation.createDocument('', '', null);
        this.mStyleDoc.addEventListener("load",this.styleLoaded(),false);
        this.mStyleDoc.load(xalan_fl+".xsl");
    },

    // nsIWebProgressListener
    QueryInterface: function(aIID)
    {
        return this;
    },
    onStateChange: function(aProg, aRequest, aFlags, aStatus)
    {
        if ((aFlags & nsIWebProgListener.STATE_STOP) &&
            (aFlags & nsIWebProgListener.STATE_IS_DOCUMENT)) {
            aProg.removeProgressListener(this);
            this.mRefDoc = document.getElementById('hiddenHtml').contentDocument;
            this.fileLoaded(4);
        }
    },
    onProgressChange: function(aProg, b,c,d,e,f)
    {
    },
    onLocationChange: function(aProg, aRequest, aURI, aFlags)
    {
    },
    onStatusChange: function(aProg, aRequest, aStatus, aMessage)
    {
    },
    onSecurityChange: function(aWebProgress, aRequest, aState)
    {
    },

    // onload handler helper
    onload : function(file)
    {
        var self = this;
        return function(e)
        {
            return self.fileLoaded(file);
        };
    },

    styleLoaded : function()
    {
        var self = this;
        return function(e)
        {
            return self.styleLoadedHelper();
        };
    },
    styleLoadedHelper : function()
    {
        var method = methodExpr.evaluate(this.mStyleDoc.documentElement, 2,
                                         null).stringValue;
        var refContent;
        if (!method) {
            // implicit method, guess from result
            refContent = this.loadTextFile(this.mRefURL);
            if (refContent.match(/^\s*<html/gi)) {
                method = 'html';
            }
            else {
                method = 'xml';
            }
        }
        this.mMethod = method;

        switch (method) {
        case 'xml':
            if (!refContent) {
                refContent = this.loadTextFile(this.mRefURL);
            }
            this.mRefDoc = parser.parseFromString(refContent, 'application/xml');
            this.mLoaded += 4;
            break;
        case 'html':
            view.loadHtml(this.mRefURL, this);
            break;
        case 'text':
            if (!refContent) {
                refContent = this.loadTextFile(this.mRefURL);
            }
            const ns = 'http://www.mozilla.org/TransforMiix';
            const qn = 'transformiix:result';
            this.mRefDoc =
                document.implementation.createDocument(ns, qn, null);
            var txt = this.mRefDoc.createTextNode(refContent);
            this.mRefDoc.documentElement.appendChild(txt);
            this.mLoaded += 4;
            break;
        default:
            throw "unkown XSLT output method";
        }
        this.fileLoaded(2)
    },

    fileLoaded : function(mask)
    {
        this.mLoaded += mask;
        if (this.mLoaded < 7) {
            return;
        }
        this.doTransform();
    },

    doTransform : function()
    {
        this.kProcessor.reset();
        try {
            this.kProcessor.importStylesheet(this.mStyleDoc);
            this.mResDoc =
                this.kProcessor.transformToDocument(this.mSourceDoc);
            this.mRefDoc.normalize();
            isGood = DiffDOM(this.mResDoc.documentElement,
                             this.mRefDoc.documentElement,
                             this.mMethod == 'html');
        } catch (e) {
            isGood = false;
        };
        dump("This succeeded. "+isGood+"\n");
        isGood = isGood.toString();
        for (var i=0; i<this.kObservers.length; i++) {
            var aObs = this.kObservers[i];
            if (typeof(aObs)=='object' && typeof(aObs.observe)=='function') {
                aObs.observe(this.mResource, 'success', isGood);
            }
            else if (typeof(aObs)=='function') {
                aObs(this.mResource, 'success', isGood);
            }
        }
        if (this.mObserver) {
            if (typeof(this.mObserver)=='object') {
                this.mObserver.observe(this.mResource, 'success', isGood);
            }
            else {
                this.mObserver(this.mResource, 'success', isGood);
            }
        }
    },

    loadTextFile : function(url)
    {
        var serv = Components.classes[IOSERVICE_CTRID].
            getService(nsIIOService);
        if (!serv) {
            throw Components.results.ERR_FAILURE;
        }
        var chan = serv.newChannel(url, null, null);
        var instream = doCreate(SIS_CTRID, nsISIS);
        instream.init(chan.open());

        return instream.read(instream.available());
    }
}

runItem.prototype.kXalan.QueryInterface(nsIStandardURL);

var cmdTestController = 
{
    supportsCommand: function(aCommand)
    {
        switch(aCommand) {
            case 'cmd_tst_run':
            case 'cmd_tst_runall':
                return true;
            default:
        }
        return false;
    },
    isCommandEnabled: function(aCommand)
    {
        return this.supportsCommand(aCommand);
    },
    doCommand: function(aCommand)
    {
        switch(aCommand) {
            case 'cmd_tst_run':
                dump("cmd_tst_run\n");
                break;
            case 'cmd_tst_runall':
                dump("cmd_tst_runall\n");
                var tst_run = document.getElementById('cmd_tst_run');
                tst_run.doCommand();
            default:
        }
    }
};

registerController(cmdTestController);

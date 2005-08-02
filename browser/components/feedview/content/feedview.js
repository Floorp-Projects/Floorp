/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Feedview for Firefox.
 *
 * The Initial Developer of the Original Code is
 * Tom Germeau <tom.germeau@epigoon.com>.
 * Portions created by the Initial Developer are Copyright (C) 2005
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var curLength;
var number ;

// Add the name of the feed to the title.
function setFeed() {
  var title = document.getElementsByTagName("h1")[0].textContent;
  document.title = document.getElementsByTagName("title")[0].textContent + " " +  title;
}

// Try to get a Date() from a date string.
function xmlDate(In) {
  In = In.replace('Z', '+00:00')
  var D = In.replace(/^(\d{4})-(\d\d)-(\d\d)T([0-9:]*)([.0-9]*)(.)(\d\d):(\d\d)$/, '$1/$2/$3 $4 $6$7$8');
  D = Date.parse(D);
  D += 1000*RegExp.$5;
  return new Date(D);
}

function setDate() {
  // Find all items (div) in the htmlfeed.
  var divs = document.getElementsByTagName("div");
  for (var i = 0; i < divs.length; i++) {
    // If the current div element has a date...
    var d = divs[i].getAttribute("date");
    if (d) {
      // If it is an RFC... date -> first parse it...
      // otherwise try the Date() constructor.
      if (d.indexOf("T"))
        d = xmlDate( d );
      else
        d = new Date( d );

      // If the date could be parsed...
      if (d instanceof Date) {
        // XXX It would be nicer to say day = "Today" or "Yesterday".
        var day = d.toGMTString();
        day = day.substring(0, 11);

        divs[i].getElementsByTagName("span")[0].textContent =
          day + " @ " + lead(d.getHours()) +  ":" + lead(d.getMinutes());
      }
    }
  } // end of the divs loop
}

// XXX This function should not exist.
// It tries to fix as many special characters as posible.
function fixchars(txt) {
  txt = txt.replace(/&nbsp;/g, " ");
  txt = txt.replace(/&amp;/g, "&");
  txt = txt.replace(/&gt;/g, ">");
  txt = txt.replace(/&lt;/g, "<");
  txt = txt.replace(/&quot;/g, "'");
  txt = txt.replace(/&#8217;/g, "'");
  txt = txt.replace(/&#8216;/g, "'");
  txt = txt.replace(/&#8212;/g, "—");
  txt = txt.replace(/&#33;/g, "!");
  txt = txt.replace(/&#38;/g, "&");
  txt = txt.replace(/&#39;/g, "'");
  return txt;
}

// This function is called when the page is loaded and when the feeds are resized.
// maxLength: maximum length (in words) of the article, which can be set by the slider
// init: is this onload or not
function updateArticleLength(maxLength, init) {
  // regex to get all the img tags
  var img = /<img([^>]*)>/g;
  var imgArr; 
  var im;
  
  // sl: the slider element
  var sl = document.getElementById("lengthSlider");
  sl.setAttribute("curpos", maxLength);
  
  // performance trick, don't know if it actually speeds up
  if (maxLength == curLength) return;
  curLength = maxLength;

  var divs = document.getElementsByTagName("div");
  // for each feed item (div) in the feedhtml
  for (var i = 0; i < divs.length; i++) {
    // if this is onload, set the title (the <a>) of each feed item
    if (init) {
      var title = divs[i].getElementsByTagName("a");
      if (title.length > 0) { // just being safe
        title = title[0];
        title.textContent = fixchars(title.textContent);
        // ...and remove all html tags from the title.
        title.textContent = title.textContent.replace(/<([^>]*)>/g, "");
      }
    }

    var txt = divs[i].getAttribute("description");
    // If the item contains a description attribute, fill the <p> with it.
    if (txt != null) {
      txt = fixchars(txt);
  
      // Replace all <br> and <p> by real breaks.
      txt = txt.replace(/<br[^\>]*>/g, "\n");
      txt = txt.replace(/<p[^\>]*>/g, "\n");
  
      // If we have a description AND we are in onload
      // find all images in the description and put them in an array
      // before they are stripped as common html tags.
      if (init) {
        imgArr = new Array();
        img.lastIndex = 0;
        
        while ((im = img.exec(txt)) != null) {
          var src = /\< *[img][^\>]*[src] *= *[\"\']{0,1}([^\"\'\ >]*)/i;
          im[0] = im[0].replace('border=', ''); // fix somtin?
          im[0] = im[0].replace('class=', ''); // fix somtin?
          var imgsrc = src.exec(im[0]);
          imgArr.push(imgsrc[1]);
        }

        // For each found image in the description create a new <img> tag 
        // and append it after the <p> with the description.
        var o; // will be our new <img> tag
        for (im=0; im < imgArr.length; im++) {
          a = document.createElementNS("http://www.w3.org/1999/xhtml", "a");
          a.setAttribute("href", imgArr[im]);
          o = document.createElementNS("http://www.w3.org/1999/xhtml", "img");
          a.setAttribute("class", "image");
          o.setAttribute("src", imgArr[im]);
          a.appendChild(o);
          divs[i].appendChild(a);
        }
      } // end:if (init)
  
      // Kill all html.
      txt = txt.replace(/<([^>]*)>/g, "");
      /* split the description in words*/
      var ar = txt.split(" ");
  
      // performance... :)
      if (maxLength == 0) txt = "";
      
      if (maxLength < 100 && maxLength != 0) {
        if (ar.length > maxLength) {
          txt = "";
          // append the number of words (maxLength)
          for (var x = 0; x < maxLength ; x++) {
            txt += ar[x] + " ";
          }
          txt += " ..." ;
        }
      }
  
      // the <p> of the feeditem we are working with
      var currentP = divs[i].getElementsByTagName("p")[0];
      if (currentP != null) {
        // Remove all previous elements from the parent <p>,
        // which fill in the next lines of code.
        for (var pc = currentP.childNodes.length; pc > 0; pc--)
          currentP.removeChild(currentP.childNodes[0]);
        
        // Split our description and for each line (which were 
        // actually <br> or <p>) of the description add a new <p>.
        var p = txt.split("\n");
        for (im = 0; im < p.length; im++) {
          if (p != "") {
            var a = document.createElementNS("http://www.w3.org/1999/xhtml", "p");
            a.textContent = p[im];
            currentP.appendChild(a);
          }
        }
      } // end if (currentP != null)
    } // end if (txt != null)
  } // end for (i=0; i<divs.length; i++)  
}

// leading zero function
function lead(In) {
  if (In < 10)
    return "0" + In; 
  else
    return In;
}

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is aboutHome.xhtml.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (original author)
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

// If a definition requires additional params, check that the final search url
// is handled correctly by the engine.
const SEARCH_ENGINES = {
  "Google": {
    image: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEYAAAAYCAMAAABwdHsxAAADAFBMVEVogZYwjM9NtOspS2u0KAfYZBb3rRTiurCMjYrUWhIaPlzDNgfVRgjaVgvoizTI9f2ta17/9Wzrs0v46M/+7VOW6vrJQAsBLochdr+nmF/++o+UDQK88vzaYQarq6scarXT+f68vLzjdRrjewp5d3WqGATkUwaTq9uk7PnUaydkICTJSBD4uCDgZAz9+LiK6PjRZFH33Xv01qbrlxznhgvwx5gCBCbOUhLjfCzhaxHztXG8RhX93UP5yTRGl9QhRGT5wigkRmXniRj70zoKWrT2yUdqjs3UhzXvl0JwU0teu+EBElGD2fH83FZo1vTPilT0vDmd0Z3dbhrWrHLhcQir8PzTTDfPrVGrajv03cf5+vkwUG6z8PrSUw571uzvpCGPTzBsPDMxMEcHagXrkg7voBHf/P8FGHHhzrbyryOQZAeDNCudnZyTJBL2zlT+xByoUCG1wspyY1nRiH7STA3yiB7Jjx/pqT/Pz8/7lhDvfBxTUkl95PY/RE0DI2SQbT7LzJjxdg1OMTzutynnm2v+5DWsNBX+/tVDqOa7tnX57OPGYSHysziEo5a3XBe3l3j+0SXCoaCsjTL++fMgQmLmxXdv0ezsv0ycQxf8p0X1hwrenDU/Tnj9//tkyNj3+fX89fDv22QXHEWQydee7/xEdKG84uRWi5bwaxyK3/L++/iU5PMWM2Q6GS7UXg81l9gmSGgXOVewsKO3x+ECK8h8x3j7/fs5bMBke6kuTmwoRV/SbArKeSrB1snY373x5aSIRz85YIvragDD5+lnorUvSWF80PCQr6o4KGKysrIeQmIgXJ1Vb4j///8+XXjs7ewaUZSutsKFkqY4T3IhXqgYSHtwiJwUNVLExcbk5eU4SGcrcLEsP1wPKV77+/vy8/NBkcBmrN+drLklNGIONHQ5WXVxyuk0VXHHz9dmxuonZahTrNqPobEcP19CVXMURIljzvE/odz29vYcP3VieZHo6OhCYHxOW29y2vNYwe79/f33+PnU2eExgcLX2Nje397NKLzjAAACjElEQVR42q2UMWvbQBTHPRgro6CDKIJOPQSeVH+CDuZ2E3BIVmNkBB5agTF0NYV00y7jdMji0XqxvNljAzmEPASrQ0Vwhk41tPZiC6l3OjmNXYeYujfo3nF3v/fe/+le5hX5HyOTzv1Of88bnXelq0q8C9NRHMcpFrHd3gPTD0qV17uisWeDqE269gyguU88pcrRDowNXrqYTt1/xkQwXi8mZ2Q/TO9vDNa2FVkJLUPocDsYDoe1lB6EURzZ4RrDThHi0RF7mRh+bFGirCEYCJnMLpzUase5XI2aZxhjKM6wS64YJlDZqWxAPEq6ztjFm01KEzGdTxGirgoNFpRfrU7IEg8IiQFoqRlmqYp0p4VGDEO8zB1sYoLyKJlFVCdxjotwntNpISJqYfjKMQKi1fDVvJlqg2G6gWmh98l8iuRoWL1O7Bc53b2TJoSFc88xquwKqig8SGyDFvxRMGyriEdDUF140+DR9HN6hGHOBJIsjinLpvHoV84EIPGCL/ClAzOiojzfKeejz9WX3H6ru98Td6GWaqOi3lbBQQsTlyzzOU1ajjimTgpUkxRDN7Uvtl1kmTGMgT5+28BcAAC/SBSJBl7mWS1kkwRVnpWvs8n1HP6nMswFQmJyp71+DK4mgZ0ssEarFsiyQe0sg9V03acnG2luClgcQx+DKaOyTwTjAUNiS5OKWFGwZbGYw5GsiqKYNI6erp8XTo7TGigAToecVSofjvrEzMuyPBIe9xvXux2MvXCZZroQhNU6a7/mJ5VsK87l+FaD2TLpTvTTbTbj7bb13PiJZ0zAT+u6PtH9nhuKxD270s0hGAfSBOCgaJLHxDrlffcQzOoXSKyYjksOwdDWOLcG2H5i8zf27La6kVLrqwAAAABJRU5ErkJggg=="
  , params: "source=hp&channel=np"
  , links: {
      advanced: "http://www.google.com/advanced_search"
    , preferences: "http://www.google.com/preferences"
    }
  }

, "Яндекс":
  {
    image: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGQAAAArCAMAAAC5Mt3fAAABnlBMVEX/////AAD/AwP/Bgb/CQn/DAz/Dw//EhL/FRX/GBj/Gxv/Hh7/ISH/JCT/Jyf/MDD/MzP/Pz//QkL/SEj/S0v/Tk7/UVH/YGD/Zmb/aWn/bGz/cnL/dXX/eHj/e3v/fn7/hIT/jY3/k5P/lpb/mZn/nJz/n5//paX/q6v/rq7/tLT/w8P/xsb/z8//1dX/2Nj/29v/3t7/4eH/5OT/7e3/8PD/8/P/9vb/+fn//Pz8/Pz5+fn29vbz8/Pw8PDt7e3q6urn5+fk5OTh4eHe3t7b29vY2NjS0tLPz8/MzMzJycnGxsbDw8PAwMC9vb26urq0tLSurq6rq6uoqKilpaWioqKfn5+cnJyZmZmWlpaTk5ONjY2KioqHh4eEhISBgYF+fn57e3t4eHh1dXVycnJvb29sbGxpaWlmZmZjY2NgYGBdXV1aWlpXV1dUVFRRUVFOTk5LS0tISEhFRUVCQkI/Pz88PDw5OTk2NjYzMzMwMDAtLS0qKionJyckJCQhISEeHh4bGxsVFRUSEhIPDw8MDAwJCQkGBgYDAwMAAABXxKaDAAAAAXRSTlMAQObYZgAAA3pJREFUWMPt1ulbG1UUx/HfQVsUrK1a9612c6t16SZBlkpTKLIkiG26KBSLoTVUdpiUQiIJyfe/9sXcYZJJSFsfx1c9b5J57p18cu5y7pWex/OIPS6cPHrkBTOzQ/EZp9rNxcHYjPfMzOzVI50xIifN7OXPz0sn4kO+e9Hs8DkpVuQDs7azihfpajd7SzEjZ8zsdNzIcTM7Gzfyvpldiht5x8z0L5BE6s6t4f2bhzJTmSt1SDSTMvwiSXdhw28aJAj35vAmwGKPJOkeLEvSzyUYk6S+BQBWRh3yoZl9HUGAm5KUhUd+08geMuj/0ZL/tCTXb01Sd8H/VN9j1/meQ043WV1NkarnPQ6QRB7WU9O4jB1yAxiWpAXYuTGeo9gTbHgze/NpkJKfz6AkXYdSn3QfNkMksQ3zkjQO1VFJ6bG9SXnNrO3Mk5AUFGuQDXgg6RqQ3EMmoZKUpFVYiKyDL82s85snI1shMgCkJSXKkAmQxCbMyrWmmhQvO3js25bIBGyEyKTLQHn4I0DS/hBKN6H6Q8Oaftes/tBqRDLwV4jMQiUhScv++srCmjz4VZL0595bdfFpRwOSy2QymdW97lOQDZEFKEqSFmHdISkodsul93C/KvlKeDIShkPmYDpE8rDrVcIOWfDybjlLJTc30XjD7MD5ujmJIEswESKFSIcs7AK3/HoDzDQxvjCzj+snPjJcBUiGyN9QmHvoeZ7nefNuuBZxe6+3OdJ12Kz9QqvV1QvlRIjswGLtD2RhbRi4I0l9NB2uE2Z2rOU+GYdVhci2q1G1iJZhp9cN13yDcbHD7KWLLZEZf3kGyDpsNyAjwLT8PPMNyEdm9knrHe/5JT5A7gP9UUQrUO6XtALVvohx7oBZx6WWSBJWpPodf7sBGXO1fQa/2NTG22Z2vHWBvOvOogDp2YXiQBTRBlR+lIaAYn0qX7WZdX7fEukpuXkeCQ6MLFC4GkVSwJykNWBjoBZ53cxOtTxPbm9B0fM8/9AqPpB0uQSwPDfvlSdCRB5Uk9JQBagszS3mC+E+PNTVEslRFwVJGqsEj7M1SBrISRrdCVovS5KOmrV9Fp7E7ZKkKkyGF4lmiIYeAVD6vd+/SKwEBbN6VdLgll9y0k99+cm5vNytxQ2BfspMXb/Wvd9LY5nfJpLPcDPbB/lv439BspTHg+9XKv417hniH6z+4JNllI0iAAAAAElFTkSuQmCC"
  }
};

// The process of adding a new default snippet involves:
//   * add a new entity to aboutHome.dtd
//   * add a <span/> for it in aboutHome.xhtml
//   * add an entry here in the proper ordering (based on spans)
// The <a/> part of the snippet will be linked to the corresponding url.
const DEFAULT_SNIPPETS_URLS = [
  "http://www.mozilla.com/firefox/4.0/features"
, "https://addons.mozilla.org/firefox/?browse=featured"
];

const SNIPPETS_UPDATE_INTERVAL_MS = 86400000; // 1 Day.

let gSearchEngine;

function onLoad(event)
{
  setupSearchEngine();
  document.getElementById("searchText").focus();

  loadSnippets();
}


function onSearchSubmit(aEvent)
{
  let searchTerms = document.getElementById("searchText").value;
  if (gSearchEngine && searchTerms.length > 0) {
    const SEARCH_TOKENS = {
      "_searchTerms_": encodeURIComponent(searchTerms)
    }
    let url = gSearchEngine.searchUrl;
    for (let key in SEARCH_TOKENS) {
      url = url.replace(key, SEARCH_TOKENS[key]);
    }
    window.location.href = url;
  }

  aEvent.preventDefault();
}


function setupSearchEngine()
{
  gSearchEngine = JSON.parse(localStorage["search-engine"]);

  // Look for extended information, like logo and links.
  let searchEngineInfo = SEARCH_ENGINES[gSearchEngine.name];
  if (searchEngineInfo) {
    for (let prop in searchEngineInfo)
      gSearchEngine[prop] = searchEngineInfo[prop];
  }

  // Enqueue additional params if required by the engine definition.
  if (gSearchEngine.params)
    gSearchEngine.searchUrl += "&" + gSearchEngine.params;

  // Add search engine logo.
  if (gSearchEngine.image) {
    let logoElt = document.getElementById("searchEngineLogo");
    logoElt.src = gSearchEngine.image;
    logoElt.alt = gSearchEngine.name;
  }

  if (gSearchEngine.links) {
    // Add search engine links.
    if (gSearchEngine.links.advanced) {
      let advancedLink = document.getElementById("searchEngineAdvancedLink");
      advancedLink.setAttribute("href", gSearchEngine.links.advanced);
      advancedLink.hidden = false;
    }
    if (gSearchEngine.links.preferences) {
      let prefsLink = document.getElementById("searchEngineAdvancedPreferences");
      prefsLink.setAttribute("href", gSearchEngine.links.preferences);
      prefsLink.hidden = false;
    }
  }
}

function loadSnippets()
{
  // Check last snippets update.
  let lastUpdate = localStorage["snippets-last-update"];
  let updateURL = localStorage["snippets-update-url"];
  if (updateURL && (!lastUpdate ||
                    Date.now() - lastUpdate > SNIPPETS_UPDATE_INTERVAL_MS)) {
    // Try to update from network.
    let xhr = new XMLHttpRequest();
    xhr.open('GET', updateURL, true);
    xhr.onerror = function (event) {
      showSnippets();
    };
    xhr.onload = function (event)
    {
      if (xhr.status == 200) {
        localStorage["snippets"] = xhr.responseText;
        localStorage["snippets-last-update"] = Date.now();
      }
      showSnippets();
    };
    xhr.send(null);
  } else {
    showSnippets();
  }
}

function showSnippets()
{
  let snippets = localStorage["snippets"];
  if (snippets) {
    let snippetsElt = document.getElementById("snippets");
    snippetsElt.innerHTML = snippets;
    // Scripts injected by innerHTML are inactive, so we have to relocate them
    // through DOM manipulation to activate their contents.
    Array.forEach(snippetsElt.getElementsByTagName("script"), function(elt) {
      let relocatedScript = document.createElement("script");
      relocatedScript.type = "text/javascript;version=1.8";
      relocatedScript.text = elt.text;
      elt.parentNode.replaceChild(relocatedScript, elt);
    });
    snippetsElt.hidden = false;
  } else {
    // If there are no saved snippets, show one of the default ones.
    let defaultSnippetsElt = document.getElementById("defaultSnippets");
    let entries = defaultSnippetsElt.querySelectorAll("span");
    // Choose a random snippet.  Assume there is always at least one.
    let randIndex = Math.round(Math.random() * (entries.length - 1));
    let entry = entries[randIndex];
    // Inject url in the eventual link.
    if (DEFAULT_SNIPPETS_URLS[randIndex]) {
      let links = entry.getElementsByTagName("a");
      if (links.length != 1)
        return; // Something is messed up in this entry, we support just 1 link.
      links[0].href = DEFAULT_SNIPPETS_URLS[randIndex];
    }
    entry.hidden = false;
  }
}

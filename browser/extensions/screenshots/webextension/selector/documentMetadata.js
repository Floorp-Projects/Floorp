"use strict";

this.documentMetadata = (function() {

  function findSiteName() {
    let el = document.querySelector("meta[property='og:site_name']");
    if (el) {
      return el.getAttribute("content");
    }
    // nytimes.com uses this property:
    el = document.querySelector("meta[name='cre']");
    if (el) {
      return el.getAttribute("content");
    }
    return null;
  }

  function getOpenGraph() {
    let openGraph = {};
    // If you update this, also update _OPENGRAPH_PROPERTIES in shot.js:
    let forceSingle = `title type url`.split(/\s+/g);
    let openGraphProperties = `
    title type url image audio description determiner locale site_name video
    image:secure_url image:type image:width image:height
    video:secure_url video:type video:width image:height
    audio:secure_url audio:type
    article:published_time article:modified_time article:expiration_time article:author article:section article:tag
    book:author book:isbn book:release_date book:tag
    profile:first_name profile:last_name profile:username profile:gender
    `.split(/\s+/g);
    for (let prop of openGraphProperties) {
      let elems = document.querySelectorAll(`meta[property='og:${prop}']`);
      if (forceSingle.includes(prop) && elems.length > 1) {
        elems = [elems[0]];
      }
      let value;
      if (elems.length > 1) {
        value = [];
        for (let elem of elems) {
          let v = elem.getAttribute("content");
          if (v) {
            value.push(v);
          }
        }
        if (!value.length) {
          value = null;
        }
      } else if (elems.length === 1) {
        value = elems[0].getAttribute("content");
      }
      if (value) {
        openGraph[prop] = value;
      }
    }
    return openGraph;
  }

  function getTwitterCard() {
    let twitterCard = {};
    // If you update this, also update _TWITTERCARD_PROPERTIES in shot.js:
    let properties = `
    card site title description image
    player player:width player:height player:stream player:stream:content_type
    `.split(/\s+/g);
    for (let prop of properties) {
      let elem = document.querySelector(`meta[name='twitter:${prop}']`);
      if (elem) {
        let value = elem.getAttribute("content");
        if (value) {
          twitterCard[prop] = value;
        }
      }
    }
    return twitterCard;
  }

  return function documentMetadata() {
    let result = {};
    result.docTitle = document.title;
    result.siteName = findSiteName();
    result.openGraph = getOpenGraph();
    result.twitterCard = getTwitterCard();
    return result;
  };

})();
null;

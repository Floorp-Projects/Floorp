/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test DuckDuckGo search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";

function test() {
  let engine = Services.search.getEngineByName("DuckDuckGo");
  ok(engine, "DuckDuckGo");

  let base = "https://duckduckgo.com/?q=foo";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base + "&t=ffsb", "Check search URL for 'foo'");
  url = engine.getSubmission("foo", null, "contextmenu").uri.spec;
  is(url, base + "&t=ffcm", "Check context menu search URL for 'foo'");
  url = engine.getSubmission("foo", null, "keyword").uri.spec;
  is(url, base + "&t=ffab", "Check keyword search URL for 'foo'");
  url = engine.getSubmission("foo", null, "searchbar").uri.spec;
  is(url, base + "&t=ffsb", "Check search bar search URL for 'foo'");
  url = engine.getSubmission("foo", null, "homepage").uri.spec;
  is(url, base + "&t=ffhp", "Check homepage search URL for 'foo'");
  url = engine.getSubmission("foo", null, "newtab").uri.spec;
  is(url, base + "&t=ffnt", "Check newtab search URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "https://ac.duckduckgo.com/ac/?q=foo&type=list", "Check search suggestion URL for 'foo'");

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "DuckDuckGo",
    alias: null,
    description: "Search DuckDuckGo",
    searchForm: "https://duckduckgo.com/?q=&t=ffsb",
    hidden: false,
    wrappedJSObject: {
      queryCharset: "UTF-8",
      "_iconURL": "data:image/icon;base64,AAABAAIAEBAAAAEAIABoBAAAJgAAACAgAAABACAAqBAAAI4EAAAoAAAAEAAAACAAAAABACAAAAAAAAAEAAATCwAAEwsAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA11RgALs6oACbQ9wAj0v8AI9L/ACfQ9wAu0agANdUYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAzzN4CNdL/oK/z//////////////////////+jsPv/BDXX/wAz0t4AAAAAAAAAAAAAAAAAAAAAAAAAAAAyzvNSduD//////8jK/v+P+Lf/IbQL/17RPP+J3Y//wOKX//////9YeuX/ADLO8wAAAAAAAAAAAAAAAAAw091piOX/8/X9/1Fx5P9xhu//WOWZ/0W9Lv9Lwjn/J8BB/xyDAP9bdfL/9fP//2mI5v8AMNPdAAAAAAc610YRQ9f//////0Zr4P8AGdD/sb32////////////wrv//wAh1/8MPab/ACPc/05r4///////EkPX/wc610YANtWkrr/y/6S48P8AJ9L/AB3R/+/w/v///////////3+D7f8AQeL/AYTw/wFr5/8AMNb/p7Tv/6698v8AM9WkADLW//////8yXt//AC3V/wAw1/////////////z///8A0P7/AKb1/wWI7P8AuPf/AJ3w/zZW3P//////ADHV/wAx2P//////AzrZ/wAu1/84ZOL////////////e////AND//wC1+f8Atff/AZbv/wY62f8ELNf//////wAw1/8AMtn//////wAw2f8ALNn/kKrz////+//cwbH////////////R////Rcb8/wDO/f8A/P//AHzo//////8AMNj/ADXa//////8vXuL/ACna/4yq9///79T/jUkg/9i+r///////r2Q0/7Cozv8BKdr/AirY/zdZ4P//////ADTa/wI72tOuv/T/prr0/wAl2v+JqPb//7yW/+bUxv/9+/n////u//W+n/+Op/L/ADPd/wAv2v+ru/T/r7/0/wI72tMLQd1DEEjg//////9Cbef/ADng///////////////////////R3///AC3g/wAy3v9SeOn//////xFI4P8LQd1DAAAAAAM64PNmiuz/9/j//2mN7f/m7P3///////////9Cb+n/ACXd/wAt3v9rju3//////2iL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/P8J0vz/ELDz/xt85v8mSdj/JknY/yZJ2P8lStowJUjXYCZJ2P8nSdhwJErZryZK2f8oSNcgJUnajyZK2f8mStn/JkrZ/yZK2f8mStn/iJPA////////////////////////////0ff+/xjV/P8J0vz/Drn1/xiO6/8Yjuv/GI7r/xCw8/8Lyvr/CdL8/xmF6P8mStn/JkrZ/yVJ2o8oSNcgJkrZ/yRK2a8jStrfI0rZ3wAAAAAlSdq/Jkra/yZK2v8mStr/Jkra/yZK2v+xtsf///////////////////////////8o2Pz/CdL8/wvK+v8mStr/Jkra/yZK2v8mStr/Jkra/yZK2v8iW97/Jkra/yZK2v8mStr/JUnavwAAAAAjStnfI0ra3yZK2v8lSdq/AAAAACZH2O8mStr/Jkra/yZK2v8mStr/L1HY/9HR0f///////////////////////////yjY/P8J0vz/CdL8/xCw9P8QsPT/ELD0/xSf7/8ddeX/Jkra/yZK2v8mStr/Jkra/yZK2v8mR9jvAAAAACVJ2r8mStr/Jkvb/yVJ2r8AAAAAJkvb/yZL2/8mS9v/Jkvb/yZL2/9KZtL/4+Pj////////////////////////////4Pn//0fd/f8J0vz/CdL8/wnS/P8J0vz/CdL8/wnS/P8Lyvr/Fpfu/yJc3/8mS9v/Jkvb/yZL2/8AAAAAJUnavyZL2/8mS9z/JUncvwAAAAAmS9z/Jkvc/yZL3P8mS9z/Jkvc/26AyP/x8fH//////////////////////////////////////9H3/v/C9P7/o+7+/2fa+/8Oufb/CdL8/wnS/P8J0vz/CdL8/xiP7P8mS9z/Jkvc/wAAAAAlSdy/Jkvc/yZM3P8lTNy/AAAAACZJ2e8mTNz/Jkzc/yZM3P8mTNz/iJTB////////////qnth/5VaOf/x6eX///////////////////////Hp5f/x6eX/ydL2/yZM3P8kVN7/G37o/xKo8v8QsfT/HXbm/yZM3P8mSdnvAAAAACVM3L8mTNz/I0vc3yZJ2u8AAAAAJUzevyZM3f8mTN3/Jkzd/yZM3f+fqc3///////////+VWjn/v5yI/+re1///////////////////////jk8s/7iRe//J0vb/Jkzd/yZM3f8mTN3/Jkzd/yZM3f8mTN3/Jkzd/yVM3r8AAAAAI0vc3yNL3N8kTd2vJk3d/yhQ3yAlTd2PJk3d/yZN3f8mTd3/Jk3d/6St0v////////////Hp5f/q3tf///////////////////////////+xhm7/49PK/6Cx8P8mTd3/Jk3d/yZN3f8mTd3/Jk3d/yZN3f8mTd3/JU3djyhQ3yAmTd3/JE3drydN33AmTd7/J03fcCVK3zAmTd7/Jk3e/yZN3v8mTd7/pK7S///////Sp5r/////////////////////////////////////////////////T27k/yZN3v8mTd7/Jk3e/yZN3v8mTd7/Jk3e/yZN3v8lSt8wJ03fcCZN3v8nTd9wKFDfICZO3/8mTt3PAAAAACVN3r8mTt//Jk7f/yZO3/+EltX//////+fRyv/SqaD/59LO///////////////////////at63/vIBy/7Glxf8mTt//Jk7f/yZO3/8mTt//Jk7f/yZO3/8mTt//JU3evwAAAAAmTt3PJk7f/yhQ3yAAAAAAJE/dryZO3/8oUN9AKFDfQCZO3/8mTt//Jk7f/zhb2v/o6/T/////////////////////////////////////////////////XHrn/yZO3/8mTt//Jk7f/yZO3/8mTt//Jk7f/yZO3/8oUN9AKFDfQCZO3/8kT92vAAAAAAAAAAAoUN9AJk7g/yZO4M8AAAAAJk/hnyZO4P8mTuD/Jk7g/05v5v/k6fv//////////////////////////////////////3eR7P8mTuD/Jk7g/yZO4P8mTuD/Jk7g/yZO4P8mTuD/Jk/hnwAAAAAmTuDPJk7g/yhQ30AAAAAAAAAAAAAAAAAjT+GfJU/h/yVO4Y8gUN8QIk7gzyVP4f8lT+H/SWnW/0lp1v+bq+H/8fHx/////////////////6Cy8v9OcOb/JU/h/yVP4f8lT+H/JU/h/yVP4f8lT+H/JU/h/yJO4M8gUN8QJU7hjyVP4f8jT+GfAAAAAAAAAAAAAAAAAAAAACBQ3xAlTOHvJU/h/yVQ4mAgUN8QIk7hzyVP4f+ktOv///////////////////////H0/f9phur/JU/h/yVP4f8lT+H/JU/h/yVP4f8lT+H/JU/h/yVP4f8iTuHPIFDfECVQ4mAlT+H/JUzh7yBQ3xAAAAAAAAAAAAAAAAAAAAAAAAAAACVQ3zAlUOLvJVDi/yVQ4mAgUN8QI1Din4mb2//J0/j/ydP4/6299P93ku3/M1vk/yVQ4v8lUOL/JVDi/yVQ4v8lUOL/JVDi/yVQ4v8lUOL/I1DinyBQ3xAlUOJgJVDi/yVQ4u8lUN8wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACVQ5DAlUOLvJVDi/yVQ4o8AAAAAJFDjQCVQ4r8lUOL/JVDi/yVQ4v8lUOL/JVDi/yVQ4v8lUOL/JVDi/yVQ4v8lUOL/JVDivyRQ40AAAAAAJVDijyVQ4v8lUOLvJVDkMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACVQ5DAjUeTfJVHj/yNR5N8kUONAAAAAACVQ5DAmUuOAJVHivyNR5N8lUeP/JVHj/yNR5N8lUeK/JlLjgCVQ5DAAAAAAJFDjQCNR5N8lUeP/I1Hk3yVQ5DAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACBQ3xAjUuSfJVHk/yVR5P8jUeTfJFLkcChQ5yAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAoUOcgJFLkcCNR5N8lUeT/JVHk/yNS5J8gUN8QAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAkUONAI1LknyVS5P8lUuT/JVLk/yVS5O8lUeS/JVHkvyVR5L8lUeS/JVLk7yVS5P8lUuT/JVLk/yRS468kUONAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIFDfECVS5GAjUuWfIlPlzyVS5f8lUuX/JVLl/yVS5f8iU+XPI1LlnyVS5GAgUN8QAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP/AA///AAD//AAAP/ggBB/wgAEP4AAAB8AAAAPAAAADiAAAEYAAAAEQAAAIAAAAAAAAAAAgAAAEIAAABCAAAAQgAAAEIAAABCAAAAQAAAAAAAAAABAAAAiAAAABiAAAEcAAAAPAAAAD4AAAB/CAAQ/4IAQf/AfgP/8AAP//wAP/",
      _urls : [
        {
          type: "text/html",
          method: "GET",
          template: "https://duckduckgo.com/",
          params: [
            {
              name: "q",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "t",
              value: "ffcm",
              purpose: "contextmenu",
            },
            {
              name: "t",
              value: "ffab",
              purpose:"keyword",
            },
            {
              name: "t",
              value: "ffsb",
              purpose: "searchbar",
            },
            {
              name: "t",
              value: "ffhp",
              purpose: "homepage",
            },
            {
              name: "t",
              value: "ffnt",
              purpose: "newtab",
            },
          ],
          mozparams: {},
        },
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template: "https://ac.duckduckgo.com/ac/",
          params: [
            {
              name: "q",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "type",
              value: "list",
              purpose: undefined,
            },
          ],
        },
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "DuckDuckGo");
}

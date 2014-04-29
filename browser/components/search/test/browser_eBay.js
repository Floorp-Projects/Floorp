/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test eBay search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";

function test() {
  let engine = Services.search.getEngineByName("eBay");
  ok(engine, "eBay");

  let base = "http://rover.ebay.com/rover/1/711-47294-18009-3/4?mfe=search&mpre=http://www.ebay.com/sch/i.html?_nkw=foo";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base, "Check search URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "http://autosug.ebay.com/autosug?sId=0&kwd=foo&fmt=osr", "Check search suggestion URL for 'foo'");

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "eBay",
    alias: null,
    description: "eBay - Online auctions",
    searchForm: "http://search.ebay.com/",
    type: Ci.nsISearchEngine.TYPE_MOZSEARCH,
    hidden: false,
    wrappedJSObject: {
      queryCharset: "ISO-8859-1",
      "_iconURL": "data:image/x-icon;base64,AAABAAIAEBAAAAAAAAB6AQAAJgAAACAgAAAAAAAAQgMAAKABAACJUE5HDQoaCgAAAA1JSERSAAAAEAAAABAIBgAAAB/z/2EAAAFBSURBVDjLtZPdK0MBGIf3J5Babhx3rinFBWuipaUskX9DYvkopqgV90q5UJpyp0OKrUWM2VrRsS9D0zZKHGaOnW1nj4vtypVtPPe/533r9746QAAOAJXfo5Yzgg44pHrcugon/6Sgo0b+XuAOZ2iZiVQmyPoDpIwmUkYTzqM7GsdDdC7F6Lbf8pzOkfWOouzqeZem2b+2AqAV8zjD8yVBqqcf2b7C66yNiMGMfixIQSvi8Mp0LEbR5ADq1QSKWM+Gx0RC9nOZ2GLzwlIWdPWiuNzk4w/EpThNkyEAXKEP2ud8KGId2sspilhPMrmNwzfCuqePr/xbSfC5I/I0MMSj2YJ3z49gDdO2cEOrLUowJpE9G0QRG1ClKbR0EIdvmOPYcnUtnN+vsnZiQC1k/qnGagQ1n3LNzySUJZVskitnmr8BlQG7T2hvgxsAAAAASUVORK5CYIKJUE5HDQoaCgAAAA1JSERSAAAAIAAAACAIBgAAAHN6evQAAAMJSURBVFjD7ZddSFNhGMeHXXQTZFFCWfR1pRhUECQlBdWVToo+6KYu1KigtDASG5qUfZgFZvahEDosECPDktKZS1FL+1DRnEvdUptjug91X2dnZzv/3vO6OZbWnR4v9sADL+fs7P97/s/znu2VAJD4UkpSSdKG+QubTyPBr+sXz8XCR64fIAHihVTis0SsUAoAVhEBrBKIHCGAEMB/ARi3F5LkbpS2WMRzYEEBXC2tsD6T03R9agsCGLNyqPw6CXmrBT06JvhbPHZwmkdwtR0B138PPKOHgzXD5jLAy3tmibo4K9weZwDAazJj/FQKRnfugfHMeRiTz0K3Ixam1HQKcPC+Fisu9NK1P08Uj4DleHgMdXC+WQ7nu3UEOhFMfTQcVUvQ1H4IN2sj8H2k7K+2TqCc3GseyA8AmDOzMBq7D9bS8sAr6nEJdNt3UbHVF1XQGtmZew8bTPT6tWoD3KpsUvlR8NxUoEICMvl6KQo+xqCwcRs4T8Ax5c8bFExjbAgAjO7aS8VsLypgq3g5nWStjztAhWRVhqAqeB6IuKTClkw1eNYEbrCQQBwD8yGGOsAooogLYejQPKBi7UPF9DkH+ezd+o141ZkUPAOC+L9SAMivNc7q46YMNSLTe4n1kaQF4XD3ZIDTPgU3XEYciKcAHrsGJS1xKFBGgyVzouiT4VbdGhjt/cEA5isyKsaz7jl3we7bg7Rqf6j0LoSldON4wWcqJDgQNGTN++l13vELA+MK6kKd6iryFOvxtidt9i5gO7owdjKJQliflNAU1pas6xQgnAzg1ux+lJEdILixNr0Pq9JUUA8NwVG9DM73G0jlcnh+V4BpjIWzJmIGQIjnXw5TiDuKSEwxurm3ITc8DNO51BnrLbIcsrW0dNA6RxgUKU1UdGVqLy5X6qGzTLvlnewiBZyGs3Yz6X8UeaYI3olvZDhzwLumZ+eHvooCCC0Q5VUsb4unwycM4YIDqA01tPqmgbzQr2EIYPECiPm33LYoDiZSsY9moh9O/Znoa4d9HkXtPg2pX/cPKCoRQ+ocZa4AAAAASUVORK5CYII=",
      _urls : [
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template: "http://autosug.ebay.com/autosug",
          params: [
            {
              name: "sId",
              value: "0",
              purpose: undefined,
            },
            {
              name: "kwd",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "fmt",
              value: "osr",
              purpose: undefined,
            },
          ],
        },
        {
          type: "text/html",
          method: "GET",
          template: "http://rover.ebay.com/rover/1/711-47294-18009-3/4",
          params: [
            {
              name: "mfe",
              value: "search",
              purpose: undefined,
            },
            {
              name: "mpre",
              value: "http://www.ebay.com/sch/i.html?_nkw={searchTerms}",
              purpose: undefined,
            },
          ],
          mozparams: {},
        },
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "eBay");
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test rotor with heading
 */
addAccessibleTask(
  `<h1 id="hello">hello</h1><br><h2 id="world">world</h2><br>goodbye`,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXHeadingSearchKey",
      AXImmediateDescendants: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    const webArea = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    is(
      webArea.getAttributeValue("AXRole"),
      "AXWebArea",
      "Got web area accessible"
    );

    const headingCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(2, headingCount, "Found two headings");

    const headings = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    const hello = getNativeInterface(accDoc, "hello");
    const world = getNativeInterface(accDoc, "world");
    is(
      hello.getAttributeValue("AXTitle"),
      headings[0].getAttributeValue("AXTitle"),
      "Found correct first heading"
    );
    is(
      world.getAttributeValue("AXTitle"),
      headings[1].getAttributeValue("AXTitle"),
      "Found correct second heading"
    );
  }
);

/**
 * Test rotor with articles
 */
addAccessibleTask(
  `<article id="google">
  <h2>Google Chrome</h2>
  <p>Google Chrome is a web browser developed by Google, released in 2008. Chrome is the world's most popular web browser today!</p>
  </article>

  <article id="moz">
  <h2>Mozilla Firefox</h2>
  <p>Mozilla Firefox is an open-source web browser developed by Mozilla. Firefox has been the second most popular web browser since January, 2018.</p>
  </article>

  <article id="microsoft">
  <h2>Microsoft Edge</h2>
  <p>Microsoft Edge is a web browser developed by Microsoft, released in 2015. Microsoft Edge replaced Internet Explorer.</p>
  </article> `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXArticleSearchKey",
      AXImmediateDescendants: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    const webArea = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    is(
      webArea.getAttributeValue("AXRole"),
      "AXWebArea",
      "Got web area accessible"
    );

    const articleCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(3, articleCount, "Found three articles");

    const articles = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    const google = getNativeInterface(accDoc, "google");
    const moz = getNativeInterface(accDoc, "moz");
    const microsoft = getNativeInterface(accDoc, "microsoft");

    is(
      google.getAttributeValue("AXTitle"),
      articles[0].getAttributeValue("AXTitle"),
      "Found correct first article"
    );
    is(
      moz.getAttributeValue("AXTitle"),
      articles[1].getAttributeValue("AXTitle"),
      "Found correct second article"
    );
    is(
      microsoft.getAttributeValue("AXTitle"),
      articles[2].getAttributeValue("AXTitle"),
      "Found correct third article"
    );
  }
);

/**
 * Test rotor with tables
 */
addAccessibleTask(
  `
  <table id="shapes">
    <tr>
      <th>Shape</th>
      <th>Color</th>
      <th>Do I like it?</th>
    </tr>
    <tr>
      <td>Triangle</td>
      <td>Green</td>
      <td>No</td>
    </tr>
    <tr>
      <td>Square</td>
      <td>Red</td>
      <td>Yes</td>
    </tr>
  </table>
  <br>
  <table id="food">
    <tr>
      <th>Grocery Item</th>
      <th>Quantity</th>
    </tr>
    <tr>
      <td>Onions</td>
      <td>2</td>
    </tr>
    <tr>
      <td>Yogurt</td>
      <td>1</td>
    </tr>
    <tr>
      <td>Spinach</td>
      <td>1</td>
    </tr>
    <tr>
      <td>Cherries</td>
      <td>12</td>
    </tr>
    <tr>
      <td>Carrots</td>
      <td>5</td>
    </tr>
  </table>
  <br>
  <div role="table" id="ariaTable">
    <div role="row">
      <div role="cell">
        I am a tiny aria table
      </div>
    </div>
  </div>
  <br>
  <table role="grid" id="grid">
    <tr>
      <th>A</th>
      <th>B</th>
      <th>C</th>
      <th>D</th>
      <th>E</th>
    </tr>
    <tr>
      <th>F</th>
      <th>G</th>
      <th>H</th>
      <th>I</th>
      <th>J</th>
    </tr>
  </table>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXTableSearchKey",
      AXImmediateDescendants: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    const webArea = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    is(
      webArea.getAttributeValue("AXRole"),
      "AXWebArea",
      "Got web area accessible"
    );

    const tableCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(4, tableCount, "Found four tables");

    const tables = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    const shapes = getNativeInterface(accDoc, "shapes");
    const food = getNativeInterface(accDoc, "food");
    const ariaTable = getNativeInterface(accDoc, "ariaTable");
    const grid = getNativeInterface(accDoc, "grid");

    is(
      shapes.getAttributeValue("AXColumnCount"),
      tables[0].getAttributeValue("AXColumnCount"),
      "Found correct first table"
    );
    is(
      food.getAttributeValue("AXColumnCount"),
      tables[1].getAttributeValue("AXColumnCount"),
      "Found correct second table"
    );
    is(
      ariaTable.getAttributeValue("AXColumnCount"),
      tables[2].getAttributeValue("AXColumnCount"),
      "Found correct third table"
    );
    is(
      grid.getAttributeValue("AXColumnCount"),
      tables[3].getAttributeValue("AXColumnCount"),
      "Found correct fourth table"
    );
  }
);

/**
 * Test rotor with landmarks
 */
addAccessibleTask(
  `
  <header id="header">
    <h1>This is a heading within a header</h1>
  </header>

  <nav id="nav">
    <a href="example.com">I am a link in a nav</a>
  </nav>

  <main id="main">
    I am some text in a main element
  </main>

  <footer id="footer">
    <h2>Heading in footer</h2>
  </footer>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXLandmarkSearchKey",
      AXImmediateDescendants: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    const webArea = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    is(
      webArea.getAttributeValue("AXRole"),
      "AXWebArea",
      "Got web area accessible"
    );

    const landmarkCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(4, landmarkCount, "Found four landmarks");

    const landmarks = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    const header = getNativeInterface(accDoc, "header");
    const nav = getNativeInterface(accDoc, "nav");
    const main = getNativeInterface(accDoc, "main");
    const footer = getNativeInterface(accDoc, "footer");

    is(
      header.getAttributeValue("AXSubrole"),
      landmarks[0].getAttributeValue("AXSubrole"),
      "Found correct first landmark"
    );
    is(
      nav.getAttributeValue("AXSubrole"),
      landmarks[1].getAttributeValue("AXSubrole"),
      "Found correct second landmark"
    );
    is(
      main.getAttributeValue("AXSubrole"),
      landmarks[2].getAttributeValue("AXSubrole"),
      "Found correct third landmark"
    );
    is(
      footer.getAttributeValue("AXSubrole"),
      landmarks[3].getAttributeValue("AXSubrole"),
      "Found correct fourth landmark"
    );
  }
);

/**
 * Test rotor with aria landmarks
 */
addAccessibleTask(
  `
  <div id="banner" role="banner">
    <h1>This is a heading within a banner</h1>
  </div>

  <div id="nav" role="navigation">
    <a href="example.com">I am a link in a nav</a>
  </div>

  <div id="main" role="main">
    I am some text in a main element
  </div>

  <div id="contentinfo" role="contentinfo">
    <h2>Heading in contentinfo</h2>
  </div>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXLandmarkSearchKey",
      AXImmediateDescendants: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    const webArea = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    is(
      webArea.getAttributeValue("AXRole"),
      "AXWebArea",
      "Got web area accessible"
    );

    const landmarkCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(4, landmarkCount, "Found four landmarks");

    const landmarks = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    const banner = getNativeInterface(accDoc, "banner");
    const nav = getNativeInterface(accDoc, "nav");
    const main = getNativeInterface(accDoc, "main");
    const contentinfo = getNativeInterface(accDoc, "contentinfo");

    is(
      banner.getAttributeValue("AXSubrole"),
      landmarks[0].getAttributeValue("AXSubrole"),
      "Found correct first landmark"
    );
    is(
      nav.getAttributeValue("AXSubrole"),
      landmarks[1].getAttributeValue("AXSubrole"),
      "Found correct second landmark"
    );
    is(
      main.getAttributeValue("AXSubrole"),
      landmarks[2].getAttributeValue("AXSubrole"),
      "Found correct third landmark"
    );
    is(
      contentinfo.getAttributeValue("AXSubrole"),
      landmarks[3].getAttributeValue("AXSubrole"),
      "Found correct fourth landmark"
    );
  }
);

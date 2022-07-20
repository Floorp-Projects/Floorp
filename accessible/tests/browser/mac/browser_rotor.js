/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/states.js */
loadScripts({ name: "states.js", dir: MOCHITESTS_DIR });

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});

/**
 * Test rotor with heading
 */
addAccessibleTask(
  `<h1 id="hello">hello</h1><br><h2 id="world">world</h2><br>goodbye`,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXHeadingSearchKey",
      AXImmediateDescendantsOnly: 1,
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
      AXImmediateDescendantsOnly: 1,
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
      AXImmediateDescendantsOnly: 1,
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
      AXImmediateDescendantsOnly: 1,
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
      AXImmediateDescendantsOnly: 1,
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

/**
 * Test rotor with buttons
 */
addAccessibleTask(
  `
  <button id="button">hello world</button><br>

  <input type="button" value="another kinda button" id="input"><br>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXButtonSearchKey",
      AXImmediateDescendantsOnly: 1,
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

    const buttonCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(2, buttonCount, "Found two buttons");

    const buttons = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    const button = getNativeInterface(accDoc, "button");
    const input = getNativeInterface(accDoc, "input");

    is(
      button.getAttributeValue("AXRole"),
      buttons[0].getAttributeValue("AXRole"),
      "Found correct button"
    );
    is(
      input.getAttributeValue("AXRole"),
      buttons[1].getAttributeValue("AXRole"),
      "Found correct input button"
    );
  }
);

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
 * Test rotor with buttons
 */
addAccessibleTask(
  `
  <form>
    <h2>input[type=button]</h2>
    <input type="button" value="apply" id="button1">

    <h2>input[type=submit]</h2>
    <input type="submit" value="submit now" id="submit">

    <h2>input[type=image]</h2>
    <input type="image" src="sample.jpg" alt="submit image" id="image">

    <h2>input[type=reset]</h2>
    <input type="reset" value="reset now" id="reset">

    <h2>button element</h2>
    <button id="button2">Submit button</button>
  </form>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXControlSearchKey",
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

    const controlsCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(5, controlsCount, "Found 5 controls");

    const controls = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    const button1 = getNativeInterface(accDoc, "button1");
    const submit = getNativeInterface(accDoc, "submit");
    const image = getNativeInterface(accDoc, "image");
    const reset = getNativeInterface(accDoc, "reset");
    const button2 = getNativeInterface(accDoc, "button2");

    is(
      button1.getAttributeValue("AXTitle"),
      controls[0].getAttributeValue("AXTitle"),
      "Found correct first control"
    );
    is(
      submit.getAttributeValue("AXTitle"),
      controls[1].getAttributeValue("AXTitle"),
      "Found correct second control"
    );
    is(
      image.getAttributeValue("AXTitle"),
      controls[2].getAttributeValue("AXTitle"),
      "Found correct third control"
    );
    is(
      reset.getAttributeValue("AXTitle"),
      controls[3].getAttributeValue("AXTitle"),
      "Found correct third control"
    );
    is(
      button2.getAttributeValue("AXTitle"),
      controls[4].getAttributeValue("AXTitle"),
      "Found correct third control"
    );
  }
);

/**
 * Test rotor with inputs
 */
addAccessibleTask(
  `
  <input type="text" value="I'm a text field." id="text"><br>
  <input type="text" value="me too" id="implText"><br>
  <textarea id="textarea">this is some text in a text area</textarea><br>
  <input type="tel" value="0000000000" id="tel"><br>
  <input type="url" value="https://example.com" id="url"><br>
  <input type="email" value="hi@example.com" id="email"><br>
  <input type="password" value="blah" id="password"><br>
  <input type="month" value="2020-01" id="month"><br>
  <input type="week" value="2020-W01" id="week"><br>
  <input type="number" value="12" id="number"><br>
  <input type="range" value="12" min="0" max="20" id="range"><br>
  <input type="date" value="2020-01-01" id="date"><br>
  <input type="time" value="10:10:10" id="time"><br>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXControlSearchKey",
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

    const controlsCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(13, controlsCount, "Found 13 controls");
    // the extra controls here come from our time control
    // we can't filter out its internal buttons/incrementors
    // like we do with the date entry because the time entry
    // doesn't have its own specific role -- its just a grouping.

    const controls = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const text = getNativeInterface(accDoc, "text");
    const implText = getNativeInterface(accDoc, "implText");
    const textarea = getNativeInterface(accDoc, "textarea");
    const tel = getNativeInterface(accDoc, "tel");
    const url = getNativeInterface(accDoc, "url");
    const email = getNativeInterface(accDoc, "email");
    const password = getNativeInterface(accDoc, "password");
    const month = getNativeInterface(accDoc, "month");
    const week = getNativeInterface(accDoc, "week");
    const number = getNativeInterface(accDoc, "number");
    const range = getNativeInterface(accDoc, "range");

    const toCheck = [
      text,
      implText,
      textarea,
      tel,
      url,
      email,
      password,
      month,
      week,
      number,
      range,
    ];

    for (let i = 0; i < toCheck.length; i++) {
      is(
        toCheck[i].getAttributeValue("AXValue"),
        controls[i].getAttributeValue("AXValue"),
        "Found correct input control"
      );
    }

    const date = getNativeInterface(accDoc, "date");
    const time = getNativeInterface(accDoc, "time");

    is(
      date.getAttributeValue("AXRole"),
      controls[11].getAttributeValue("AXRole"),
      "Found corrent date editor"
    );

    is(
      time.getAttributeValue("AXRole"),
      controls[12].getAttributeValue("AXRole"),
      "Found corrent time editor"
    );
  }
);

/**
 * Test rotor with groupings
 */
addAccessibleTask(
  `
  <fieldset>
    <legend>Radios</legend>
    <div role="radiogroup" id="radios">
      <input id="radio1" type="radio" name="g1" checked="checked"> Radio 1
      <input id="radio2" type="radio" name="g1"> Radio 2
    </div>
  </fieldset>

  <fieldset id="checkboxes">
    <legend>Checkboxes</legend>
      <input id="checkbox1" type="checkbox" name="g2"> Checkbox 1
      <input id="checkbox2" type="checkbox" name="g2" checked="checked">Checkbox 2
  </fieldset>

  <fieldset id="switches">
    <legend>Switches</legend>
      <input id="switch1" name="g3" role="switch" type="checkbox">Switch 1
      <input checked="checked" id="switch2" name="g3" role="switch" type="checkbox">Switch 2
  </fieldset>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXControlSearchKey",
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

    const controlsCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(9, controlsCount, "Found 9 controls");

    const controls = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const radios = getNativeInterface(accDoc, "radios");
    const radio1 = getNativeInterface(accDoc, "radio1");
    const radio2 = getNativeInterface(accDoc, "radio2");

    is(
      radios.getAttributeValue("AXRole"),
      controls[0].getAttributeValue("AXRole"),
      "Found correct group of radios"
    );
    is(
      radio1.getAttributeValue("AXRole"),
      controls[1].getAttributeValue("AXRole"),
      "Found correct radio 1"
    );
    is(
      radio2.getAttributeValue("AXRole"),
      controls[2].getAttributeValue("AXRole"),
      "Found correct radio 2"
    );

    const checkboxes = getNativeInterface(accDoc, "checkboxes");
    const checkbox1 = getNativeInterface(accDoc, "checkbox1");
    const checkbox2 = getNativeInterface(accDoc, "checkbox2");

    is(
      checkboxes.getAttributeValue("AXRole"),
      controls[3].getAttributeValue("AXRole"),
      "Found correct group of checkboxes"
    );
    is(
      checkbox1.getAttributeValue("AXRole"),
      controls[4].getAttributeValue("AXRole"),
      "Found correct checkbox 1"
    );
    is(
      checkbox2.getAttributeValue("AXRole"),
      controls[5].getAttributeValue("AXRole"),
      "Found correct checkbox 2"
    );

    const switches = getNativeInterface(accDoc, "switches");
    const switch1 = getNativeInterface(accDoc, "switch1");
    const switch2 = getNativeInterface(accDoc, "switch2");

    is(
      switches.getAttributeValue("AXRole"),
      controls[6].getAttributeValue("AXRole"),
      "Found correct group of switches"
    );
    is(
      switch1.getAttributeValue("AXRole"),
      controls[7].getAttributeValue("AXRole"),
      "Found correct switch 1"
    );
    is(
      switch2.getAttributeValue("AXRole"),
      controls[8].getAttributeValue("AXRole"),
      "Found correct switch 2"
    );
  }
);

/**
 * Test rotor with misc controls
 */
addAccessibleTask(
  `
  <input role="spinbutton" id="spinbutton" type="number" value="25">

  <details id="details">
    <summary>Hello</summary>
    world
  </details>

  <ul role="tree" id="tree">
    <li role="treeitem">item1</li>
    <li role="treeitem">item1</li>
  </ul>

  <a id="buttonMenu" role="button">Click Me</a>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXControlSearchKey",
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

    const controlsCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(4, controlsCount, "Found 4 controls");

    const controls = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const spin = getNativeInterface(accDoc, "spinbutton");
    const details = getNativeInterface(accDoc, "details");
    const tree = getNativeInterface(accDoc, "tree");
    const buttonMenu = getNativeInterface(accDoc, "buttonMenu");

    is(
      spin.getAttributeValue("AXRole"),
      controls[0].getAttributeValue("AXRole"),
      "Found correct spinbutton"
    );
    is(
      details.getAttributeValue("AXRole"),
      controls[1].getAttributeValue("AXRole"),
      "Found correct details element"
    );
    is(
      tree.getAttributeValue("AXRole"),
      controls[2].getAttributeValue("AXRole"),
      "Found correct tree"
    );
    is(
      buttonMenu.getAttributeValue("AXRole"),
      controls[3].getAttributeValue("AXRole"),
      "Found correct button menu"
    );
  }
);

/**
 * Test rotor with links
 */
addAccessibleTask(
  `
  <a href="" id="empty">empty link</a>
  <a href="http://www.example.com/" id="href">Example link</a>
  <a id="noHref">link without href</a>
  `,
  async (browser, accDoc) => {
    let searchPred = {
      AXSearchKey: "AXLinkSearchKey",
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

    let linkCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(2, linkCount, "Found two links");

    let links = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    const empty = getNativeInterface(accDoc, "empty");
    const href = getNativeInterface(accDoc, "href");

    is(
      empty.getAttributeValue("AXTitle"),
      links[0].getAttributeValue("AXTitle"),
      "Found correct first link"
    );
    is(
      href.getAttributeValue("AXTitle"),
      links[1].getAttributeValue("AXTitle"),
      "Found correct second link"
    );

    // unvisited links

    searchPred = {
      AXSearchKey: "AXUnvisitedLinkSearchKey",
      AXImmediateDescendants: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    linkCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(2, linkCount, "Found two links");

    links = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(
      empty.getAttributeValue("AXTitle"),
      links[0].getAttributeValue("AXTitle"),
      "Found correct first link"
    );
    is(
      href.getAttributeValue("AXTitle"),
      links[1].getAttributeValue("AXTitle"),
      "Found correct second link"
    );

    // visited links

    let stateChanged = waitForEvent(EVENT_STATE_CHANGE, "href");

    await PlacesTestUtils.addVisits(["http://www.example.com/"]);

    await stateChanged;

    searchPred = {
      AXSearchKey: "AXVisitedLinkSearchKey",
      AXImmediateDescendants: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    linkCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(1, linkCount, "Found one link");

    links = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(
      href.getAttributeValue("AXTitle"),
      links[0].getAttributeValue("AXTitle"),
      "Found correct visited link"
    );

    // Ensure history is cleared before running again
    await PlacesUtils.history.clear();
  }
);

/*
 * Test AXAnyTypeSearchKey with root group
 */
addAccessibleTask(
  `<h1 id="hello">hello</h1><br><h2 id="world">world</h2><br>goodbye`,
  (browser, accDoc) => {
    let searchPred = {
      AXSearchKey: "AXAnyTypeSearchKey",
      AXImmediateDescendantsOnly: 1,
      AXResultsLimit: 1,
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

    let results = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(results.length, 1, "One result for root group");
    is(
      results[0].getAttributeValue("AXIdentifier"),
      "root-group",
      "Is generated root group"
    );

    searchPred.AXStartElement = results[0];
    results = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(results.length, 0, "No more results past root group");

    searchPred.AXDirection = "AXDirectionPrevious";
    results = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(
      results.length,
      0,
      "Searching backwards from root group should yield no results"
    );

    const rootGroup = webArea.getAttributeValue("AXChildren")[0];
    is(
      rootGroup.getAttributeValue("AXIdentifier"),
      "root-group",
      "Is generated root group"
    );

    searchPred = {
      AXSearchKey: "AXAnyTypeSearchKey",
      AXImmediateDescendantsOnly: 1,
      AXResultsLimit: 1,
      AXDirection: "AXDirectionNext",
    };

    results = rootGroup.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(
      results[0].getAttributeValue("AXRole"),
      "AXHeading",
      "Is first heading child"
    );
  }
);

/**
 * Test rotor with checkboxes
 */
addAccessibleTask(
  `
  <fieldset id="checkboxes">
    <legend>Checkboxes</legend>
      <input id="checkbox1" type="checkbox" name="g2"> Checkbox 1
      <input id="checkbox2" type="checkbox" name="g2" checked="checked">Checkbox 2
      <div id="checkbox3" role="checkbox">Checkbox 3</div>
      <div id="checkbox4" role="checkbox" aria-checked="true">Checkbox 4</div>
  </fieldset>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXCheckBoxSearchKey",
      AXImmediateDescendantsOnly: 0,
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

    const checkboxCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(4, checkboxCount, "Found 4 checkboxes");

    const checkboxes = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const checkbox1 = getNativeInterface(accDoc, "checkbox1");
    const checkbox2 = getNativeInterface(accDoc, "checkbox2");
    const checkbox3 = getNativeInterface(accDoc, "checkbox3");
    const checkbox4 = getNativeInterface(accDoc, "checkbox4");

    is(
      checkbox1.getAttributeValue("AXValue"),
      checkboxes[0].getAttributeValue("AXValue"),
      "Found correct checkbox 1"
    );
    is(
      checkbox2.getAttributeValue("AXValue"),
      checkboxes[1].getAttributeValue("AXValue"),
      "Found correct checkbox 2"
    );
    is(
      checkbox3.getAttributeValue("AXValue"),
      checkboxes[2].getAttributeValue("AXValue"),
      "Found correct checkbox 3"
    );
    is(
      checkbox4.getAttributeValue("AXValue"),
      checkboxes[3].getAttributeValue("AXValue"),
      "Found correct checkbox 4"
    );
  }
);

/**
 * Test rotor with radiogroups
 */
addAccessibleTask(
  `
  <div role="radiogroup" id="radios" aria-labelledby="desc">
    <h1 id="desc">some radio buttons</h1>
    <div id="radio1" role="radio"> Radio 1</div>
    <div id="radio2" role="radio"> Radio 2</div>
  </div>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXRadioGroupSearchKey",
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

    const radiogroupCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(1, radiogroupCount, "Found 1 radio group");

    const controls = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const radios = getNativeInterface(accDoc, "radios");

    is(
      radios.getAttributeValue("AXDescription"),
      controls[0].getAttributeValue("AXDescription"),
      "Found correct group of radios"
    );
  }
);

/*
 * Test rotor with inputs
 */
addAccessibleTask(
  `
  <input type="text" value="I'm a text field." id="text"><br>
  <input type="text" value="me too" id="implText"><br>
  <textarea id="textarea">this is some text in a text area</textarea><br>
  <input type="tel" value="0000000000" id="tel"><br>
  <input type="url" value="https://example.com" id="url"><br>
  <input type="email" value="hi@example.com" id="email"><br>
  <input type="password" value="blah" id="password"><br>
  <input type="month" value="2020-01" id="month"><br>
  <input type="week" value="2020-W01" id="week"><br>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXTextFieldSearchKey",
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

    const textfieldCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(9, textfieldCount, "Found 9 fields");

    const fields = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const text = getNativeInterface(accDoc, "text");
    const implText = getNativeInterface(accDoc, "implText");
    const textarea = getNativeInterface(accDoc, "textarea");
    const tel = getNativeInterface(accDoc, "tel");
    const url = getNativeInterface(accDoc, "url");
    const email = getNativeInterface(accDoc, "email");
    const password = getNativeInterface(accDoc, "password");
    const month = getNativeInterface(accDoc, "month");
    const week = getNativeInterface(accDoc, "week");

    const toCheck = [
      text,
      implText,
      textarea,
      tel,
      url,
      email,
      password,
      month,
      week,
    ];

    for (let i = 0; i < toCheck.length; i++) {
      is(
        toCheck[i].getAttributeValue("AXValue"),
        fields[i].getAttributeValue("AXValue"),
        "Found correct input control"
      );
    }
  }
);

/**
 * Test rotor with static text
 */
addAccessibleTask(
  `
  <h1>Hello I am a heading</h1>
  This is some regular text.<p>this is some paragraph text</p><br>
  This is a list:<ul>
    <li>List item one</li>
    <li>List item two</li>
  </ul>

  <a href="http://example.com">This is a link</a>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXStaticTextSearchKey",
      AXImmediateDescendants: 0,
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

    const textCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(7, textCount, "Found 7 pieces of text");

    const text = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(
      "Hello I am a heading",
      text[0].getAttributeValue("AXValue"),
      "Found correct text node for heading"
    );
    is(
      "This is some regular text.",
      text[1].getAttributeValue("AXValue"),
      "Found correct text node"
    );
    is(
      "this is some paragraph text",
      text[2].getAttributeValue("AXValue"),
      "Found correct text node for paragraph"
    );
    is(
      "This is a list:",
      text[3].getAttributeValue("AXValue"),
      "Found correct text node for pre-list text node"
    );
    is(
      "List item one",
      text[4].getAttributeValue("AXValue"),
      "Found correct text node for list item one"
    );
    is(
      "List item two",
      text[5].getAttributeValue("AXValue"),
      "Found correct text node for list item two"
    );
    is(
      "This is a link",
      text[6].getAttributeValue("AXValue"),
      "Found correct text node for link"
    );
  }
);

/**
 * Test rotor with lists
 */
addAccessibleTask(
  `
  <ul id="unordered">
    <li>hello</li>
    <li>world</li>
  </ul>

  <ol id="ordered">
    <li>item one</li>
    <li>item two</li>
  </ol>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXListSearchKey",
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

    const listCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(2, listCount, "Found 2 lists");

    const lists = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const ordered = getNativeInterface(accDoc, "ordered");
    const unordered = getNativeInterface(accDoc, "unordered");

    is(
      unordered.getAttributeValue("AXChildren")[0].getAttributeValue("AXTitle"),
      lists[0].getAttributeValue("AXChildren")[0].getAttributeValue("AXTitle"),
      "Found correct unordered list"
    );
    is(
      ordered.getAttributeValue("AXChildren")[0].getAttributeValue("AXTitle"),
      lists[1].getAttributeValue("AXChildren")[0].getAttributeValue("AXTitle"),
      "Found correct ordered list"
    );
  }
);

/*
 * Test rotor with images
 */
addAccessibleTask(
  `
  <img id="img1" alt="image one" src="http://example.com/a11y/accessible/tests/mochitest/moz.png"><br>
  <a href="http://example.com">
    <img id="img2" alt="image two" src="http://example.com/a11y/accessible/tests/mochitest/moz.png">
  </a>
  <img src="" id="img3">
  `,
  (browser, accDoc) => {
    let searchPred = {
      AXSearchKey: "AXImageSearchKey",
      AXImmediateDescendantsOnly: 0,
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

    let images = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(images.length, 3, "Found three images");

    const img1 = getNativeInterface(accDoc, "img1");
    const img2 = getNativeInterface(accDoc, "img2");
    const img3 = getNativeInterface(accDoc, "img3");

    is(
      img1.getAttributeValue("AXDescription"),
      images[0].getAttributeValue("AXDescription"),
      "Found correct image"
    );

    is(
      img2.getAttributeValue("AXDescription"),
      images[1].getAttributeValue("AXDescription"),
      "Found correct image"
    );

    is(
      img3.getAttributeValue("AXDescription"),
      images[2].getAttributeValue("AXDescription"),
      "Found correct image"
    );
  }
);

/**
 * Test rotor with frames
 */
addAccessibleTask(
  `
  <iframe id="frame1" src="data:text/html,<h1>hello</h1>world"></iframe>
  <iframe id="frame2" src="data:text/html,<iframe id='frame3' src='data:text/html,<h1>goodbye</h1>'>"></iframe>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXFrameSearchKey",
      AXImmediateDescendantsOnly: 0,
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

    const frameCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(3, frameCount, "Found 3 frames");
  }
);

/**
 * Test rotor with static text
 */
addAccessibleTask(
  `
  <h1>Hello I am a heading</h1>
  This is some regular text.<p>this is some paragraph text</p><br>
  This is a list:<ul>
    <li>List item one</li>
    <li>List item two</li>
  </ul>

  <a href="http://example.com">This is a link</a>
  `,
  async (browser, accDoc) => {
    const searchPred = {
      AXSearchKey: "AXStaticTextSearchKey",
      AXImmediateDescendants: 0,
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

    const textCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(7, textCount, "Found 7 pieces of text");

    const text = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    is(
      "Hello I am a heading",
      text[0].getAttributeValue("AXValue"),
      "Found correct text node for heading"
    );
    is(
      "This is some regular text.",
      text[1].getAttributeValue("AXValue"),
      "Found correct text node"
    );
    is(
      "this is some paragraph text",
      text[2].getAttributeValue("AXValue"),
      "Found correct text node for paragraph"
    );
    is(
      "This is a list:",
      text[3].getAttributeValue("AXValue"),
      "Found correct text node for pre-list text node"
    );
    is(
      "List item one",
      text[4].getAttributeValue("AXValue"),
      "Found correct text node for list item one"
    );
    is(
      "List item two",
      text[5].getAttributeValue("AXValue"),
      "Found correct text node for list item two"
    );
    is(
      "This is a link",
      text[6].getAttributeValue("AXValue"),
      "Found correct text node for link"
    );
  }
);

/**
 * Test search with non-webarea root
 */
addAccessibleTask(
  `
  <div id="searchroot"><p id="p1">hello</p><p id="p2">world</p></div>
  <div><p>goodybe</p></div>
  `,
  async (browser, accDoc) => {
    let searchPred = {
      AXSearchKey: "AXAnyTypeSearchKey",
      AXImmediateDescendantsOnly: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
    };

    const searchRoot = getNativeInterface(accDoc, "searchroot");
    const resultCount = searchRoot.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(resultCount, 2, "Found 2 items");

    const p1 = getNativeInterface(accDoc, "p1");
    searchPred = {
      AXSearchKey: "AXAnyTypeSearchKey",
      AXImmediateDescendantsOnly: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
      AXStartElement: p1,
    };

    let results = searchRoot.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    Assert.deepEqual(
      results.map(r => r.getAttributeValue("AXDOMIdentifier")),
      ["p2"],
      "Result is next group sibling"
    );

    searchPred = {
      AXSearchKey: "AXAnyTypeSearchKey",
      AXImmediateDescendantsOnly: 1,
      AXResultsLimit: -1,
      AXDirection: "AXDirectionPrevious",
    };

    results = searchRoot.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    Assert.deepEqual(
      results.map(r => r.getAttributeValue("AXDOMIdentifier")),
      ["p2", "p1"],
      "A reverse search should return groups in reverse"
    );
  }
);

/**
 * Test search text
 */
addAccessibleTask(
  `
  <p>It's about the future, isn't it?</p>
  <p>Okay, alright, Saturday is good, Saturday's good, I could spend a week in 1955.</p>
  <ul>
    <li>I could hang out, you could show me around.</li>
    <li>There's that word again, heavy.</li>
  </ul>
  `,
  async (browser, f, accDoc) => {
    let searchPred = {
      AXSearchKey: "AXAnyTypeSearchKey",
      AXResultsLimit: -1,
      AXDirection: "AXDirectionNext",
      AXSearchText: "could",
    };

    const webArea = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    is(
      webArea.getAttributeValue("AXRole"),
      "AXWebArea",
      "Got web area accessible"
    );

    const textSearchCount = webArea.getParameterizedAttributeValue(
      "AXUIElementCountForSearchPredicate",
      NSDictionary(searchPred)
    );
    is(textSearchCount, 2, "Found 2 matching items in text search");

    const results = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    info(results.map(r => r.getAttributeValue("AXMozDebugDescription")));

    Assert.deepEqual(
      results.map(r => r.getAttributeValue("AXValue")),
      [
        "Okay, alright, Saturday is good, Saturday's good, I could spend a week in 1955.",
        "I could hang out, you could show me around.",
      ],
      "Correct text search results"
    );
  },
  { topLevel: false, iframe: true, remoteIframe: true }
);

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
    is(3, controlsCount, "Found 3 controls");

    const controls = webArea.getParameterizedAttributeValue(
      "AXUIElementsForSearchPredicate",
      NSDictionary(searchPred)
    );

    const spin = getNativeInterface(accDoc, "spinbutton");
    const details = getNativeInterface(accDoc, "details");
    const tree = getNativeInterface(accDoc, "tree");

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
  }
);

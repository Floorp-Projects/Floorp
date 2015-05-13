#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

const PREF_INTRO_SHOWN = "browser.newtabpage.introShown";
const PREF_UPDATE_INTRO_SHOWN = "browser.newtabpage.updateIntroShown";

// These consts indicate the type of intro/onboarding we show.
const WELCOME = "welcome";
const UPDATE = "update";

// The maximum paragraph ID listed for 'newtab.intro.paragraph'
// strings in newTab.properties
const MAX_PARAGRAPH_ID = 9;

const NUM_INTRO_PAGES = 3;

let gIntro = {
  _enUSStrings: {
    "newtab.intro.paragraph2": "In order to provide this service, Mozilla collects and uses certain analytics information relating to your use of the tiles in accordance with our %1$S.",
    "newtab.intro.paragraph4": "You can turn off this feature by clicking the gear (%1$S) button and selecting 'Show blank page' in the %2$S menu.",
    "newtab.intro.paragraph5": "New Tab will show the sites you visit most frequently, along with sites we think might be of interest to you. To get started, you'll see several sites from Mozilla.",
    "newtab.intro.paragraph6": "You can %1$S or %2$S any site by using the controls available on rollover.",
    "newtab.intro.paragraph7": "Some of the sites you will see may be suggested by Mozilla and may be sponsored by a Mozilla partner. We'll always indicate which sites are sponsored.",
    "newtab.intro.paragraph8": "Firefox will only show sites that most closely match your interests on the Web. %1$S",
    "newtab.intro.paragraph9": "Now when you open New Tab, you'll also see sites we think might be interesting to you.",
    "newtab.intro.controls": "New Tab Controls",
    "newtab.learn.link2": "More about New Tab",
    "newtab.privacy.link2": "About your privacy",
    "newtab.intro.remove": "remove",
    "newtab.intro.pin": "pin",
    "newtab.intro.header.welcome": "Welcome to New Tab on %1$S",
    "newtab.intro.header.update": "New Tab got an update!",
    "newtab.intro.skip": "Skip this",
    "newtab.intro.continue": "Continue tour",
    "newtab.intro.back": "Back",
    "newtab.intro.next": "Next",
    "newtab.intro.gotit": "Got it!",
    "newtab.intro.firefox": "Firefox!"
  },

  _nodeIDSuffixes: [
    "panel",
    "what",
    "mask",
    "modal",
    "numerical-progress",
    "text",
    "buttons",
    "header",
    "footer"
  ],

  /**
   * The paragraphs & buttons to show on each page in the intros.
   *
   * _introPages.welcome and _introPages.update contain an array of
   * indices of paragraphs to be used to lookup text in _paragraphs
   * for each page of the intro.
   *
   * Similarly, _introPages.buttons is used to lookup text for buttons
   * on each page of the intro.
   */
  _introPages: {
    "welcome": [[0,1],[2,3],[4,5]],
    "update": [[6,5],[4,3],[0,1]],
    "buttons": [["skip", "continue"],["back", "next"],["back", "gotit"]]
  },

  _paragraphs: [],

  _nodes: {},

  init: function() {
    for (let idSuffix of this._nodeIDSuffixes) {
      this._nodes[idSuffix] = document.getElementById("newtab-intro-" + idSuffix);
    }

    if (DirectoryLinksProvider.locale != "en-US") {
      this._nodes.what.style.display = "block";
      this._nodes.panel.addEventListener("popupshowing", e => this._setUpPanel());
      this._nodes.panel.addEventListener("popuphidden", e => this._hidePanel());
      this._nodes.what.addEventListener("click", e => this.showPanel());
    }
  },

  _goToPage: function(pageNum) {
    this._currPage = pageNum;

    this._nodes["numerical-progress"].innerHTML = `${this._bold(pageNum + 1)} / ${NUM_INTRO_PAGES}`;
    this._nodes["numerical-progress"].setAttribute("page", pageNum);

    // Set the paragraphs
    let paragraphNodes = this._nodes.text.getElementsByTagName("p");
    let paragraphIDs = this._introPages[this._onboardingType][pageNum];
    paragraphIDs.forEach((arg, index) => {
      paragraphNodes[index].innerHTML = this._paragraphs[arg];
    });

    // Set the buttons
    let buttonNodes = this._nodes.buttons.getElementsByTagName("input");
    let buttonIDs = this._introPages.buttons[pageNum];
    buttonIDs.forEach((arg, index) => {
      buttonNodes[index].setAttribute("value", this._newTabString("intro." + arg));
    });
  },

  _bold: function(str) {
    return `<strong>${str}</strong>`
  },

  _link: function(url, text) {
    return `<a href="${url}" target="_blank">${text}</a>`;
  },

  _span: function(text, className) {
    return `<span class="${className}">${text}</span>`;
  },

  _exitIntro: function() {
    this._nodes.mask.style.opacity = 0;
    this._nodes.mask.addEventListener("transitionend", () => {
      this._nodes.mask.style.display = "none";
    });
  },

  _back: function() {
    if (this._currPage == 0) {
      // We're on the first page so 'back' means exit.
      this._exitIntro();
      return;
    }
    this._goToPage(this._currPage - 1);
  },

  _next: function() {
    if (this._currPage == (NUM_INTRO_PAGES - 1)) {
      // We're on the last page so 'next' means exit.
      this._exitIntro();
      return;
    }
    this._goToPage(this._currPage + 1);
  },

  _generateParagraphs: function() {
    let customizeIcon = '<input type="button" class="newtab-control newtab-customize"/>';

    let substringMappings = {
      "2": [this._link(TILES_PRIVACY_LINK, newTabString("privacy.link"))],
      "4": [customizeIcon, this._bold(this._newTabString("intro.controls"))],
      "6": [this._bold(this._newTabString("intro.remove")), this._bold(this._newTabString("intro.pin"))],
      "7": [this._link(TILES_INTRO_LINK, newTabString("learn.link"))],
      "8": [this._link(TILES_INTRO_LINK, newTabString("learn.link"))]
    }

    for (let i = 1; i <= MAX_PARAGRAPH_ID; i++) {
      try {
        this._paragraphs.push(this._newTabString("intro.paragraph" + i, substringMappings[i]))
      } catch (ex) {
        // Paragraph with this ID doesn't exist so continue
      }
    }
  },

  _newTabString: function(str, substrArr) {
    let regExp = /%[0-9]\$S/g;
    let paragraph = this._enUSStrings["newtab." + str];

    if (!paragraph) {
      throw new Error("Paragraph doesn't exist");
    }

    let matches;
    while ((matches = regExp.exec(paragraph)) !== null) {
      let match = matches[0];
      let index = match.charAt(1); // Get the digit in the regExp.
      paragraph = paragraph.replace(match, substrArr[index - 1]);
    }
    return paragraph;
  },

  showIfNecessary: function() {
    if (!Services.prefs.getBoolPref(PREF_INTRO_SHOWN)) {
      this._onboardingType = WELCOME;
      this.showPanel();
    } else if (!Services.prefs.getBoolPref(PREF_UPDATE_INTRO_SHOWN) && DirectoryLinksProvider.locale == "en-US") {
      this._onboardingType = UPDATE;
      this.showPanel();
    }
    Services.prefs.setBoolPref(PREF_INTRO_SHOWN, true);
    Services.prefs.setBoolPref(PREF_UPDATE_INTRO_SHOWN, true);
  },

  showPanel: function() {
    if (DirectoryLinksProvider.locale != "en-US") {
      // Point the panel at the 'what' link
      this._nodes.panel.hidden = false;
      this._nodes.panel.openPopup(this._nodes.what);
      return;
    }

    this._nodes.mask.style.display = "block";
    this._nodes.mask.style.opacity = 1;

    if (!this._paragraphs.length) {
      // It's our first time showing the panel. Do some initial setup
      this._generateParagraphs();
    }
    this._goToPage(0);

    // Header text
    let boldSubstr = this._onboardingType == WELCOME ? this._span(this._newTabString("intro.firefox"), "bold") : "";
    this._nodes.header.innerHTML = this._newTabString("intro.header." + this._onboardingType, [boldSubstr]);

    // Footer links
    let footerLinkNodes = this._nodes.footer.getElementsByTagName("li");
    [this._link(TILES_INTRO_LINK, this._newTabString("learn.link2")),
     this._link(TILES_PRIVACY_LINK, this._newTabString("privacy.link2")),
    ].forEach((arg, index) => {
      footerLinkNodes[index].innerHTML = arg;
    });
  },

  _setUpPanel: function() {
    // Build the panel if necessary
    if (this._nodes.panel.childNodes.length == 1) {
      ['<a href="' + TILES_INTRO_LINK + '">' + newTabString("learn.link") + "</a>",
       '<a href="' + TILES_PRIVACY_LINK + '">' + newTabString("privacy.link") + "</a>",
       '<input type="button" class="newtab-customize"/>',
      ].forEach((arg, index) => {
        let paragraph = document.createElementNS(HTML_NAMESPACE, "p");
        this._nodes.panel.appendChild(paragraph);
        paragraph.innerHTML = newTabString("intro.paragraph" + (index + 1), [arg]);
      });
    }
  },

  _hidePanel: function() {
    this._nodes.panel.hidden = true;
  }
};

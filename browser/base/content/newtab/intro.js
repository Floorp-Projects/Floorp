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
  _nodeIDSuffixes: [
    "mask",
    "modal",
    "numerical-progress",
    "text",
    "buttons",
    "image",
    "header",
    "footer"
  ],

  _imageTypes: {
    COG : "cog",
    PIN_REMOVE : "pin-remove",
    SUGGESTED : "suggested"
  },


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
    "buttons": [["skip", "continue"],["back", "next"],["back", "gotit"]],
    "welcome-images": ["cog", "pin-remove", "suggested"],
    "update-images": ["suggested", "pin-remove", "cog"]
  },

  _paragraphs: [],

  _nodes: {},

  _images: {},

  init: function() {
    for (let idSuffix of this._nodeIDSuffixes) {
      this._nodes[idSuffix] = document.getElementById("newtab-intro-" + idSuffix);
    }

    let brand = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    this._brandShortName = brand.GetStringFromName("brandShortName");
  },

  _setImage: function(imageType) {
    // Remove previously existing images, if any.
    let currImageHolder = this._nodes.image;
    while (currImageHolder.firstChild) {
      currImageHolder.removeChild(currImageHolder.firstChild);
    }

    this._nodes.image.appendChild(this._images[imageType]);
  },

  _goToPage: function(pageNum) {
    this._currPage = pageNum;

    this._nodes["numerical-progress"].innerHTML = `${this._bold(pageNum + 1)} / ${NUM_INTRO_PAGES}`;
    this._nodes["numerical-progress"].setAttribute("page", pageNum);

    // Set the page's image
    let imageType = this._introPages[this._onboardingType + "-images"][pageNum];
    this._setImage(imageType);

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
      buttonNodes[index].setAttribute("value", newTabString("intro." + arg));
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

  _generateImages: function() {
    Object.keys(this._imageTypes).forEach(type => {
      let image = "";
      let imageClass = "";
      switch (this._imageTypes[type]) {
        case this._imageTypes.COG:
          image = document.getElementById("newtab-customize-panel").cloneNode(true);
          image.removeAttribute("hidden");
          image.removeAttribute("type");
          image.classList.add("newtab-intro-image-customize");
          break;
        case this._imageTypes.PIN_REMOVE:
          imageClass = "-hover";
          // fall-through
        case this._imageTypes.SUGGESTED:
          image = document.createElementNS(HTML_NAMESPACE, "div");
          image.classList.add("newtab-intro-cell-wrapper");

          // Create the cell's inner HTML code.
          image.innerHTML =
            '<div class="newtab-intro-cell' + imageClass + '">' +
            '  <div class="newtab-site newtab-intro-image-tile" type="sponsored">' +
            '    <a class="newtab-link">' +
            '      <span class="newtab-thumbnail"/>' +
            '      <span class="newtab-title">Example Title</span>' +
            '    </a>' +
            '    <input type="button" class="newtab-control newtab-control-pin"/>' +
            '    <input type="button" class="newtab-control newtab-control-block"/>' + (imageClass ? "" :
            '    <span class="newtab-sponsored">' + newTabString("suggested.tag") + '</span>') +
            '  </div>' +
            '</div>';
            break;
      }
      this._images[this._imageTypes[type]] = image;
    });
  },

  _generateParagraphs: function() {
    let customizeIcon = '<input type="button" class="newtab-control newtab-customize"/>';

    let substringMappings = {
      "2": [this._link(TILES_PRIVACY_LINK, newTabString("privacy.link"))],
      "4": [customizeIcon, this._bold(newTabString("intro.controls"))],
      "6": [this._bold(newTabString("intro.paragraph6.remove")), this._bold(newTabString("intro.paragraph6.pin"))],
      "7": [this._link(TILES_INTRO_LINK, newTabString("learn.link"))],
      "8": [this._brandShortName, this._link(TILES_INTRO_LINK, newTabString("learn.link"))]
    }

    for (let i = 1; i <= MAX_PARAGRAPH_ID; i++) {
      try {
        this._paragraphs.push(newTabString("intro.paragraph" + i, substringMappings[i]));
      } catch (ex) {
        // Paragraph with this ID doesn't exist so continue
      }
    }
  },

  showIfNecessary: function() {
    if (!Services.prefs.getBoolPref(PREF_INTRO_SHOWN)) {
      this._onboardingType = WELCOME;
      this.showPanel();
    } else if (!Services.prefs.getBoolPref(PREF_UPDATE_INTRO_SHOWN)) {
      this._onboardingType = UPDATE;
      this.showPanel();
    }
    Services.prefs.setBoolPref(PREF_INTRO_SHOWN, true);
    Services.prefs.setBoolPref(PREF_UPDATE_INTRO_SHOWN, true);
  },

  showPanel: function() {
    this._nodes.mask.style.display = "block";
    this._nodes.mask.style.opacity = 1;

    if (!this._paragraphs.length) {
      // It's our first time showing the panel. Do some initial setup
      this._generateParagraphs();
      this._generateImages();
    }
    this._goToPage(0);

    // Header text
    let boldSubstr = this._onboardingType == WELCOME ? this._span(this._brandShortName, "bold") : "";
    this._nodes.header.innerHTML = newTabString("intro.header." + this._onboardingType, [boldSubstr]);

    // Footer links
    let footerLinkNodes = this._nodes.footer.getElementsByTagName("li");
    [this._link(TILES_INTRO_LINK, newTabString("learn.link2")),
     this._link(TILES_PRIVACY_LINK, newTabString("privacy.link2")),
    ].forEach((arg, index) => {
      footerLinkNodes[index].innerHTML = arg;
    });
  },
};

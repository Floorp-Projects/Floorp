class _PrerenderData {
  constructor(options) {
    this.initialPrefs = options.initialPrefs;
    this.initialSections = options.initialSections;
    this._setValidation(options.validation);
  }

  get validation() {
    return this._validation;
  }

  set validation(value) {
    this._setValidation(value);
  }

  get invalidatingPrefs() {
    return this._invalidatingPrefs;
  }

    // This is needed so we can use it in the constructor
  _setValidation(value = []) {
    this._validation = value;
    this._invalidatingPrefs = value.reduce((result, next) => {
      if (typeof next === "string") {
        result.push(next);
        return result;
      } else if (next && next.oneOf) {
        return result.concat(next.oneOf);
      }
      throw new Error("Your validation configuration is not properly configured");
    }, []);
  }

  arePrefsValid(getPref) {
    for (const prefs of this.validation) {
      // {oneOf: ["foo", "bar"]}
      if (prefs && prefs.oneOf && !prefs.oneOf.some(name => getPref(name) === this.initialPrefs[name])) {
        return false;

      // "foo"
      } else if (getPref(prefs) !== this.initialPrefs[prefs]) {
        return false;
      }
    }
    return true;
  }
}

this.PrerenderData = new _PrerenderData({
  initialPrefs: {
    "migrationExpired": true,
    "showTopSites": true,
    "showSearch": true,
    "topSitesCount": 12,
    "collapseTopSites": false,
    "section.highlights.collapsed": false,
    "section.topstories.collapsed": false,
    "feeds.section.topstories": true,
    "feeds.section.highlights": true
  },
  // Prefs listed as invalidating will prevent the prerendered version
  // of AS from being used if their value is something other than what is listed
  // here. This is required because some preferences cause the page layout to be
  // too different for the prerendered version to be used. Unfortunately, this
  // will result in users who have modified some of their preferences not being
  // able to get the benefits of prerendering.
  validation: [
    "showTopSites",
    "showSearch",
    "topSitesCount",
    "collapseTopSites",
    "section.highlights.collapsed",
    "section.topstories.collapsed",
    // This means if either of these are set to their default values,
    // prerendering can be used.
    {oneOf: ["feeds.section.topstories", "feeds.section.highlights"]}
  ],
  initialSections: [
    {
      enabled: true,
      icon: "pocket",
      id: "topstories",
      order: 1,
      title: {id: "header_recommended_by", values: {provider: "Pocket"}}
    },
    {
      enabled: true,
      id: "highlights",
      icon: "highlights",
      order: 2,
      title: {id: "header_highlights"}
    }
  ]
});

this._PrerenderData = _PrerenderData;
this.EXPORTED_SYMBOLS = ["PrerenderData", "_PrerenderData"];

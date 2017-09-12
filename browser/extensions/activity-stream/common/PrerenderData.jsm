const prefConfig = {
  // Prefs listed with "invalidates: true" will prevent the prerendered version
  // of AS from being used if their value is something other than what is listed
  // here. This is required because some preferences cause the page layout to be
  // too different for the prerendered version to be used. Unfortunately, this
  // will result in users who have modified some of their preferences not being
  // able to get the benefits of prerendering.
  "migrationExpired": {value: true},
  "showTopSites": {
    value: true,
    invalidates: true
  },
  "showSearch": {
    value: true,
    invalidates: true
  },
  "topSitesCount": {value: 6},
  "feeds.section.topstories": {
    value: true,
    invalidates: true
  }
};

this.PrerenderData = {
  invalidatingPrefs: Object.keys(prefConfig).filter(key => prefConfig[key].invalidates),
  initialPrefs: Object.keys(prefConfig).reduce((obj, key) => {
    obj[key] = prefConfig[key].value;
    return obj;
  }, {}), // This creates an object of the form {prefName: value}
  initialSections: [
    {
      enabled: true,
      icon: "pocket",
      id: "topstories",
      order: 1,
      title: {id: "header_recommended_by", values: {provider: "Pocket"}},
      topics: [{}]
    }
  ]
};

this.EXPORTED_SYMBOLS = ["PrerenderData"];

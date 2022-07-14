 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["InstallRDF"];

ChromeUtils.defineModuleGetter(this, "RDFDataSource",
                               "chrome://userchromejs/content/RDFDataSource.jsm");

const RDFURI_INSTALL_MANIFEST_ROOT = "urn:mozilla:install-manifest";

function EM_R(aProperty) {
  return `http://www.mozilla.org/2004/em-rdf#${aProperty}`;
}

function getValue(literal) {
  return literal && literal.getValue();
}

function getProperty(resource, property) {
  return getValue(resource.getProperty(EM_R(property)));
}

class Manifest {
  constructor(ds) {
    this.ds = ds;
  }

  static loadFromString(text) {
    return new this(RDFDataSource.loadFromString(text));
  }
}

class InstallRDF extends Manifest {
  _readProps(source, obj, props) {
    for (let prop of props) {
      let val = getProperty(source, prop);
      if (val != null) {
        obj[prop] = val;
      }
    }
  }

  _readArrayProp(source, obj, prop, target, decode = getValue) {
    let result = Array.from(source.getObjects(EM_R(prop)),
                            target => decode(target));
    if (result.length) {
      obj[target] = result;
    }
  }

  _readArrayProps(source, obj, props, decode = getValue) {
    for (let [prop, target] of Object.entries(props)) {
      this._readArrayProp(source, obj, prop, target, decode);
    }
  }

  _readLocaleStrings(source, obj) {
    this._readProps(source, obj, ["name", "description", "creator", "homepageURL"]);
    this._readArrayProps(source, obj, {
      locale: "locales",
      developer: "developers",
      translator: "translators",
      contributor: "contributors",
    });
  }

  decode() {
    let root = this.ds.getResource(RDFURI_INSTALL_MANIFEST_ROOT);
    let result = {};

    let props = ["id", "version", "type", "updateURL", "optionsURL",
                 "optionsType", "aboutURL", "iconURL",
                 "bootstrap", "unpack", "strictCompatibility"];
    this._readProps(root, result, props);

    let decodeTargetApplication = source => {
      let app = {};
      this._readProps(source, app, ["id", "minVersion", "maxVersion"]);
      return app;
    };

    let decodeLocale = source => {
      let localized = {};
      this._readLocaleStrings(source, localized);
      return localized;
    };

    this._readLocaleStrings(root, result);

    this._readArrayProps(root, result, {"targetPlatform": "targetPlatforms"});
    this._readArrayProps(root, result, {"targetApplication": "targetApplications"},
                         decodeTargetApplication);
    this._readArrayProps(root, result, {"localized": "localized"},
                         decodeLocale);
    this._readArrayProps(root, result, {"dependency": "dependencies"},
                         source => getProperty(source, "id"));

    return result;
  }
}

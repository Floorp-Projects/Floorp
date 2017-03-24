/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu, classes: Cc, interfaces: Ci} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/CanonicalJSON.jsm");
Cu.import("resource://shield-recipe-client/lib/LogManager.jsm");
Cu.importGlobalProperties(["fetch", "URL"]); /* globals fetch, URL */

this.EXPORTED_SYMBOLS = ["NormandyApi"];

const log = LogManager.getLogger("normandy-api");
const prefs = Services.prefs.getBranch("extensions.shield-recipe-client.");

let indexPromise = null;

this.NormandyApi = {
  clearIndexCache() {
    indexPromise = null;
  },

  apiCall(method, endpoint, data = {}) {
    const url = new URL(endpoint);
    method = method.toLowerCase();

    let body = undefined;
    if (data) {
      if (method === "get") {
        for (const key of Object.keys(data)) {
          url.searchParams.set(key, data[key]);
        }
      } else if (method === "post") {
        body = JSON.stringify(data);
      }
    }

    const headers = {"Accept": "application/json"};
    return fetch(url.href, {method, body, headers});
  },

  get(endpoint, data) {
    return this.apiCall("get", endpoint, data);
  },

  post(endpoint, data) {
    return this.apiCall("post", endpoint, data);
  },

  absolutify(url) {
    const apiBase = prefs.getCharPref("api_url");
    const server = new URL(apiBase).origin;
    if (url.startsWith("http")) {
      return url;
    } else if (url.startsWith("/")) {
      return server + url;
    }
    throw new Error("Can't use relative urls");
  },

  getApiUrl: Task.async(function * (name) {
    const apiBase = prefs.getCharPref("api_url");
    if (!indexPromise) {
      indexPromise = this.get(apiBase).then(res => res.json());
    }
    const index = yield indexPromise;
    if (!(name in index)) {
      throw new Error(`API endpoint with name "${name}" not found.`);
    }
    const url = index[name];
    return this.absolutify(url);
  }),

  fetchRecipes: Task.async(function* (filters = {enabled: true}) {
    const signedRecipesUrl = yield this.getApiUrl("recipe-signed");
    const recipesResponse = yield this.get(signedRecipesUrl, filters);
    const rawText = yield recipesResponse.text();
    const recipesWithSigs = JSON.parse(rawText);

    const verifiedRecipes = [];

    for (const {recipe, signature: {signature, x5u}} of recipesWithSigs) {
      const serialized = CanonicalJSON.stringify(recipe);
      if (!rawText.includes(serialized)) {
        log.debug(rawText, serialized);
        throw new Error("Canonical recipe serialization does not match!");
      }

      const certChainResponse = yield fetch(this.absolutify(x5u));
      const certChain = yield certChainResponse.text();
      const builtSignature = `p384ecdsa=${signature}`;

      const verifier = Cc["@mozilla.org/security/contentsignatureverifier;1"]
        .createInstance(Ci.nsIContentSignatureVerifier);

      const valid = verifier.verifyContentSignature(
        serialized,
        builtSignature,
        certChain,
        "normandy.content-signature.mozilla.org"
      );
      if (!valid) {
        throw new Error("Recipe signature is not valid");
      }
      verifiedRecipes.push(recipe);
    }

    log.debug(
      `Fetched ${verifiedRecipes.length} recipes from the server:`,
      verifiedRecipes.map(r => r.name).join(", ")
    );

    return verifiedRecipes;
  }),

  /**
   * Fetch metadata about this client determined by the server.
   * @return {object} Metadata specified by the server
   */
  classifyClient: Task.async(function* () {
    const classifyClientUrl = yield this.getApiUrl("classify-client");
    const response = yield this.get(classifyClientUrl);
    const clientData = yield response.json();
    clientData.request_time = new Date(clientData.request_time);
    return clientData;
  }),

  fetchAction: Task.async(function* (name) {
    let actionApiUrl = yield this.getApiUrl("action-list");
    if (!actionApiUrl.endsWith("/")) {
      actionApiUrl += "/";
    }
    const res = yield this.get(actionApiUrl + name);
    return yield res.json();
  }),
};

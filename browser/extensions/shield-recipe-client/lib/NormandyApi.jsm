/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals URLSearchParams */

"use strict";

const {utils: Cu, classes: Cc, interfaces: Ci} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/CanonicalJSON.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.importGlobalProperties(["fetch"]); /* globals fetch */

this.EXPORTED_SYMBOLS = ["NormandyApi"];

const log = Log.repository.getLogger("extensions.shield-recipe-client");
const prefs = Services.prefs.getBranch("extensions.shield-recipe-client.");

this.NormandyApi = {
  apiCall(method, endpoint, data = {}) {
    const api_url = prefs.getCharPref("api_url");
    let url = `${api_url}/${endpoint}`;
    method = method.toLowerCase();

    if (method === "get") {
      if (data === {}) {
        const paramObj = new URLSearchParams();
        for (const key in data) {
          paramObj.append(key, data[key]);
        }
        url += "?" + paramObj.toString();
      }
      data = undefined;
    }

    const headers = {"Accept": "application/json"};
    return fetch(url, {
      body: JSON.stringify(data),
      headers,
    });
  },

  get(endpoint, data) {
    return this.apiCall("get", endpoint, data);
  },

  post(endpoint, data) {
    return this.apiCall("post", endpoint, data);
  },

  fetchRecipes: Task.async(function* (filters = {}) {
    const recipeResponse = yield this.get("recipe/signed/", filters);
    const rawText = yield recipeResponse.text();
    const recipesWithSigs = JSON.parse(rawText);

    const verifiedRecipes = [];

    for (const {recipe, signature: {signature, x5u}} of recipesWithSigs) {
      const serialized = CanonicalJSON.stringify(recipe);
      if (!rawText.includes(serialized)) {
        log.debug(rawText, serialized);
        throw new Error("Canonical recipe serialization does not match!");
      }

      const certChainResponse = yield fetch(x5u);
      const certChain = yield certChainResponse.text();
      const builtSignature = `p384ecdsa=${signature}`;

      const verifier = Cc["@mozilla.org/security/contentsignatureverifier;1"]
        .createInstance(Ci.nsIContentSignatureVerifier);

      if (!verifier.verifyContentSignature(serialized, builtSignature, certChain, "normandy.content-signature.mozilla.org")) {
        throw new Error("Recipe signature is not valid");
      }
      verifiedRecipes.push(recipe);
    }

    log.debug(`Fetched ${verifiedRecipes.length} recipes from the server:`, verifiedRecipes.map(r => r.name).join(", "));

    return verifiedRecipes;
  }),

  /**
   * Fetch metadata about this client determined by the server.
   * @return {object} Metadata specified by the server
   */
  classifyClient() {
    return this.get("classify_client/")
      .then(response => response.json())
      .then(clientData => {
        clientData.request_time = new Date(clientData.request_time);
        return clientData;
      });
  },

  fetchAction(name) {
    return this.get(`action/${name}/`).then(req => req.json());
  },
};

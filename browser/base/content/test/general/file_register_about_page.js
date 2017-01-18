const { interfaces: Ci, results: Cr, manager: Cm, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function AboutPage(chromeURL, aboutHost, classID, description, uriFlags) {
  this.chromeURL = chromeURL;
  this.aboutHost = aboutHost;
  this.classID = Components.ID(classID);
  this.description = description;
  this.uriFlags = uriFlags;
}

AboutPage.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule]),
  getURIFlags(aURI) { // eslint-disable-line no-unused-vars
    return this.uriFlags;
  },

  newChannel(aURI, aLoadInfo) {
    let newURI = Services.io.newURI(this.chromeURL);
    let channel = Services.io.newChannelFromURIWithLoadInfo(newURI,
                                                            aLoadInfo);
    channel.originalURI = aURI;

    if (this.uriFlags & Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT) {
      channel.owner = null;
    }
    return channel;
  },

  createInstance(outer, iid) {
    if (outer !== null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iid);
  },

  register() {
    Cm.QueryInterface(Ci.nsIComponentRegistrar).registerFactory(
      this.classID, this.description,
      "@mozilla.org/network/protocol/about;1?what=" + this.aboutHost, this);
  },

  unregister() {
    Cm.QueryInterface(Ci.nsIComponentRegistrar).unregisterFactory(
      this.classID, this);
  }
};

/* exported AboutPrincipalTest */
var AboutPrincipalTest = {};

XPCOMUtils.defineLazyGetter(AboutPrincipalTest, "aboutChild", () =>
  new AboutPage("chrome://mochitests/content/browser/browser/base/content/test/general/file_about_child.html",
                "test-about-principal-child",
                "{df6cbd19-c95b-4011-874b-315347c0832c}",
                "About Principal Child Test",
                Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD |
                Ci.nsIAboutModule.ALLOW_SCRIPT)

);

XPCOMUtils.defineLazyGetter(AboutPrincipalTest, "aboutParent", () =>
  new AboutPage("chrome://mochitests/content/browser/browser/base/content/test/general/file_about_parent.html",
                "test-about-principal-parent",
                "{15e1a03d-9f94-4352-bfb8-94216140d3ab}",
                "About Principal Parent Test",
                Ci.nsIAboutModule.ALLOW_SCRIPT)
);

AboutPrincipalTest.aboutParent.register();
AboutPrincipalTest.aboutChild.register();

function onUnregisterMessage() {
  removeMessageListener("AboutPrincipalTest:Unregister", onUnregisterMessage);
  AboutPrincipalTest.aboutParent.unregister();
  AboutPrincipalTest.aboutChild.unregister();
  sendAsyncMessage("AboutPrincipalTest:Unregistered");
}

addMessageListener("AboutPrincipalTest:Unregister", onUnregisterMessage);

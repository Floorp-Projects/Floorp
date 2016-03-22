/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


describe("loop.slideshow", function() {
  "use strict";

  var expect = chai.expect;
  var sharedUtils = loop.shared.utils;

  var sandbox;
  var data;

  beforeEach(function() {
    sandbox = LoopMochaUtils.createSandbox();

    LoopMochaUtils.stubLoopRequest({
      GetAllStrings: function() {
        return JSON.stringify({ textContent: "fakeText" });
      },
      GetLocale: function() {
        return "en-US";
      },
      GetPluralRule: function() {
        return 1;
      },
      GetPluralForm: function() {
        return "fakeText";
      }
    });

    data = [
      {
        id: "slide1",
        imageClass: "slide1-image",
        title: "fakeString",
        text: "fakeString"
      },
      {
        id: "slide2",
        imageClass: "slide2-image",
        title: "fakeString",
        text: "fakeString"
      },
      {
        id: "slide3",
        imageClass: "slide3-image",
        title: "fakeString",
        text: "fakeString"
      },
      {
        id: "slide4",
        imageClass: "slide4-image",
        title: "fakeString",
        text: "fakeString"
      }
    ];

    document.mozL10n.initialize({
      getStrings: function() {
        return JSON.stringify({ textContent: "fakeText" });
      },
      locale: "en-US"
    });
    sandbox.stub(document.mozL10n, "get").returns("fakeString");
    sandbox.stub(sharedUtils, "getPlatform").returns("other");
  });

  afterEach(function() {
    sandbox.restore();
    LoopMochaUtils.restore();
  });

  describe("#init", function() {
    beforeEach(function() {
      sandbox.stub(React, "render");
      sandbox.stub(document.mozL10n, "initialize");
      sandbox.stub(loop.SimpleSlideshow, "init");
    });

    it("should initalize L10n", function() {
      loop.slideshow.init();

      sinon.assert.calledOnce(document.mozL10n.initialize);
      sinon.assert.calledWith(document.mozL10n.initialize, sinon.match({ locale: "en-US" }));
    });

    it("should call the slideshow init with the right arguments", function() {
      loop.slideshow.init();

      sinon.assert.calledOnce(loop.SimpleSlideshow.init);
      sinon.assert.calledWith(loop.SimpleSlideshow.init, sinon.match("#main", data));
    });

    it("should set the document attributes correctly", function() {
      loop.slideshow.init();

      expect(document.documentElement.getAttribute("lang")).to.eql("en-US");
      expect(document.documentElement.getAttribute("dir")).to.eql("ltr");
      expect(document.body.getAttribute("platform")).to.eql("other");
    });
  });

});

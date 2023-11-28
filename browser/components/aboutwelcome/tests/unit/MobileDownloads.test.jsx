import React from "react";
import { shallow, mount } from "enzyme";
import { GlobalOverrider } from "newtab/test/unit/utils";
import { MobileDownloads } from "content-src/components/MobileDownloads";

describe("Multistage AboutWelcome MobileDownloads module", () => {
  let globals;
  let sandbox;

  beforeEach(async () => {
    globals = new GlobalOverrider();
    globals.set({
      AWFinish: () => Promise.resolve(),
      AWSendToDeviceEmailsSupported: () => Promise.resolve(),
    });
    sandbox = sinon.createSandbox();
  });

  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });

  describe("Mobile Downloads component", () => {
    const MOBILE_DOWNLOADS_PROPS = {
      data: {
        QR_code: {
          image_url:
            "chrome://browser/components/privatebrowsing/content/assets/focus-qr-code.svg",
          alt_text: {
            string_id: "spotlight-focus-promo-qr-code",
          },
        },
        email: {
          link_text: "Email yourself a link",
        },
        marketplace_buttons: ["ios", "android"],
      },
      handleAction: () => {
        window.AWFinish();
      },
    };

    it("should render MobileDownloads", () => {
      const wrapper = shallow(<MobileDownloads {...MOBILE_DOWNLOADS_PROPS} />);

      assert.ok(wrapper.exists());
    });

    it("should handle action on markeplace badge click", () => {
      const wrapper = mount(<MobileDownloads {...MOBILE_DOWNLOADS_PROPS} />);

      const stub = sandbox.stub(global, "AWFinish");
      wrapper.find(".ios button").simulate("click");
      wrapper.find(".android button").simulate("click");

      assert.calledTwice(stub);
    });

    it("should handle action on email button click", () => {
      const wrapper = shallow(<MobileDownloads {...MOBILE_DOWNLOADS_PROPS} />);

      const stub = sandbox.stub(global, "AWFinish");
      wrapper.find("button.email-link").simulate("click");

      assert.calledOnce(stub);
    });
  });
});

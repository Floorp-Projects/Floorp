/* eslint max-nested-callbacks: ["error", 100] */

import { CFRPageActions, PageAction } from "lib/CFRPageActions.jsm";
import { FAKE_RECOMMENDATION } from "./constants";
import { GlobalOverrider } from "test/unit/utils";
import { CFRMessageProvider } from "lib/CFRMessageProvider.sys.mjs";

describe("CFRPageActions", () => {
  let sandbox;
  let clock;
  let fakeRecommendation;
  let fakeHost;
  let fakeBrowser;
  let dispatchStub;
  let globals;
  let containerElem;
  let elements;
  let announceStub;
  let fakeRemoteL10n;

  const elementIDs = [
    "urlbar",
    "urlbar-input",
    "contextual-feature-recommendation",
    "cfr-button",
    "cfr-label",
    "contextual-feature-recommendation-notification",
    "cfr-notification-header-label",
    "cfr-notification-header-link",
    "cfr-notification-header-image",
    "cfr-notification-author",
    "cfr-notification-footer",
    "cfr-notification-footer-text",
    "cfr-notification-footer-filled-stars",
    "cfr-notification-footer-empty-stars",
    "cfr-notification-footer-users",
    "cfr-notification-footer-spacer",
    "cfr-notification-footer-learn-more-link",
  ];
  const elementClassNames = ["popup-notification-body-container"];

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    clock = sandbox.useFakeTimers();

    announceStub = sandbox.stub();
    const A11yUtils = { announce: announceStub };
    fakeRecommendation = { ...FAKE_RECOMMENDATION };
    fakeHost = "mozilla.org";
    fakeBrowser = {
      documentURI: {
        scheme: "https",
        host: fakeHost,
      },
      ownerGlobal: window,
    };
    dispatchStub = sandbox.stub();

    fakeRemoteL10n = {
      l10n: {},
      reloadL10n: sandbox.stub(),
      createElement: sandbox.stub().returns(document.createElement("div")),
    };

    const gURLBar = document.createElement("div");
    gURLBar.textbox = document.createElement("div");

    globals = new GlobalOverrider();
    globals.set({
      RemoteL10n: fakeRemoteL10n,
      promiseDocumentFlushed: sandbox
        .stub()
        .callsFake(fn => Promise.resolve(fn())),
      PopupNotifications: {
        show: sandbox.stub(),
        remove: sandbox.stub(),
      },
      PrivateBrowsingUtils: { isWindowPrivate: sandbox.stub().returns(false) },
      gBrowser: { selectedBrowser: fakeBrowser },
      A11yUtils,
      gURLBar,
    });
    document.createXULElement = document.createElement;

    elements = {};
    const [body] = document.getElementsByTagName("body");
    containerElem = document.createElement("div");
    body.appendChild(containerElem);
    for (const id of elementIDs) {
      const elem = document.createElement("div");
      elem.setAttribute("id", id);
      containerElem.appendChild(elem);
      elements[id] = elem;
    }
    for (const className of elementClassNames) {
      const elem = document.createElement("div");
      elem.setAttribute("class", className);
      containerElem.appendChild(elem);
      elements[className] = elem;
    }
  });

  afterEach(() => {
    CFRPageActions.clearRecommendations();
    containerElem.remove();
    sandbox.restore();
    globals.restore();
  });

  describe("PageAction", () => {
    let pageAction;

    beforeEach(() => {
      pageAction = new PageAction(window, dispatchStub);
    });

    describe("#addImpression", () => {
      it("should call _sendTelemetry with the impression payload", () => {
        const recommendation = {
          id: "foo",
          content: { bucket_id: "bar" },
        };
        sandbox.spy(pageAction, "_sendTelemetry");

        pageAction.addImpression(recommendation);

        assert.calledWith(pageAction._sendTelemetry, {
          message_id: "foo",
          bucket_id: "bar",
          event: "IMPRESSION",
        });
      });
    });

    describe("#showAddressBarNotifier", () => {
      it("should un-hideAddressBarNotifier the element and set the right label value", async () => {
        await pageAction.showAddressBarNotifier(fakeRecommendation);
        assert.isFalse(pageAction.container.hidden);
        assert.equal(
          pageAction.label.value,
          fakeRecommendation.content.notification_text
        );
      });
      it("should wait for the document layout to flush", async () => {
        sandbox.spy(pageAction.label, "getClientRects");
        await pageAction.showAddressBarNotifier(fakeRecommendation);
        assert.calledOnce(global.promiseDocumentFlushed);
        assert.callOrder(
          global.promiseDocumentFlushed,
          pageAction.label.getClientRects
        );
      });
      it("should set the CSS variable --cfr-label-width correctly", async () => {
        await pageAction.showAddressBarNotifier(fakeRecommendation);
        const expectedWidth = pageAction.label.getClientRects()[0].width;
        assert.equal(
          pageAction.urlbarinput.style.getPropertyValue("--cfr-label-width"),
          `${expectedWidth}px`
        );
      });
      it("should cause an expansion, and dispatch an impression if `expand` is true", async () => {
        sandbox.spy(pageAction, "_clearScheduledStateChanges");
        sandbox.spy(pageAction, "_expand");
        sandbox.spy(pageAction, "_dispatchImpression");

        await pageAction.showAddressBarNotifier(fakeRecommendation);
        assert.notCalled(pageAction._dispatchImpression);
        clock.tick(1001);
        assert.notEqual(
          pageAction.urlbarinput.getAttribute("cfr-recommendation-state"),
          "expanded"
        );

        await pageAction.showAddressBarNotifier(fakeRecommendation, true);
        assert.calledOnce(pageAction._clearScheduledStateChanges);
        clock.tick(1001);
        assert.equal(
          pageAction.urlbarinput.getAttribute("cfr-recommendation-state"),
          "expanded"
        );
        assert.calledOnce(pageAction._dispatchImpression);
        assert.calledWith(pageAction._dispatchImpression, fakeRecommendation);
      });
      it("should send telemetry if `expand` is true and the id and bucket_id are provided", async () => {
        await pageAction.showAddressBarNotifier(fakeRecommendation, true);
        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {
            action: "cfr_user_event",
            source: "CFR",
            message_id: fakeRecommendation.id,
            bucket_id: fakeRecommendation.content.bucket_id,
            event: "IMPRESSION",
          },
        });
      });
    });

    describe("#hideAddressBarNotifier", () => {
      it("should hideAddressBarNotifier the container, cancel any state changes, and remove the state attribute", () => {
        sandbox.spy(pageAction, "_clearScheduledStateChanges");
        pageAction.hideAddressBarNotifier();
        assert.isTrue(pageAction.container.hidden);
        assert.calledOnce(pageAction._clearScheduledStateChanges);
        assert.isNull(
          pageAction.urlbar.getAttribute("cfr-recommendation-state")
        );
      });
      it("should remove the `currentNotification`", () => {
        const notification = {};
        pageAction.currentNotification = notification;
        pageAction.hideAddressBarNotifier();
        assert.calledWith(global.PopupNotifications.remove, notification);
      });
    });

    describe("#_expand", () => {
      beforeEach(() => {
        pageAction._clearScheduledStateChanges();
        pageAction.urlbar.removeAttribute("cfr-recommendation-state");
      });
      it("without a delay, should clear other state changes and set the state to 'expanded'", () => {
        sandbox.spy(pageAction, "_clearScheduledStateChanges");
        pageAction._expand();
        assert.calledOnce(pageAction._clearScheduledStateChanges);
        assert.equal(
          pageAction.urlbarinput.getAttribute("cfr-recommendation-state"),
          "expanded"
        );
      });
      it("with a delay, should set the expanded state after the correct amount of time", () => {
        const delay = 1234;
        pageAction._expand(delay);
        // We expect that an expansion has been scheduled
        assert.lengthOf(pageAction.stateTransitionTimeoutIDs, 1);
        clock.tick(delay + 1);
        assert.equal(
          pageAction.urlbarinput.getAttribute("cfr-recommendation-state"),
          "expanded"
        );
      });
    });

    describe("#_collapse", () => {
      beforeEach(() => {
        pageAction._clearScheduledStateChanges();
        pageAction.urlbar.removeAttribute("cfr-recommendation-state");
      });
      it("without a delay, should clear other state changes and set the state to collapsed only if it's already expanded", () => {
        sandbox.spy(pageAction, "_clearScheduledStateChanges");
        pageAction._collapse();
        assert.calledOnce(pageAction._clearScheduledStateChanges);
        assert.isNull(
          pageAction.urlbarinput.getAttribute("cfr-recommendation-state")
        );
        pageAction.urlbarinput.setAttribute(
          "cfr-recommendation-state",
          "expanded"
        );
        pageAction._collapse();
        assert.equal(
          pageAction.urlbarinput.getAttribute("cfr-recommendation-state"),
          "collapsed"
        );
      });
      it("with a delay, should set the collapsed state after the correct amount of time", () => {
        const delay = 1234;
        pageAction._collapse(delay);
        clock.tick(delay + 1);
        // The state was _not_ "expanded" and so should not have been set to "collapsed"
        assert.isNull(
          pageAction.urlbar.getAttribute("cfr-recommendation-state")
        );

        pageAction._expand();
        pageAction._collapse(delay);
        // We expect that a collapse has been scheduled
        assert.lengthOf(pageAction.stateTransitionTimeoutIDs, 1);
        clock.tick(delay + 1);
        // This time it was "expanded" so should now (after the delay) be "collapsed"
        assert.equal(
          pageAction.urlbarinput.getAttribute("cfr-recommendation-state"),
          "collapsed"
        );
      });
    });

    describe("#_clearScheduledStateChanges", () => {
      it("should call .clearTimeout on all stored timeoutIDs", () => {
        pageAction.stateTransitionTimeoutIDs = [42, 73, 1997];
        sandbox.spy(pageAction.window, "clearTimeout");
        pageAction._clearScheduledStateChanges();
        assert.calledThrice(pageAction.window.clearTimeout);
        assert.calledWith(pageAction.window.clearTimeout, 42);
        assert.calledWith(pageAction.window.clearTimeout, 73);
        assert.calledWith(pageAction.window.clearTimeout, 1997);
      });
    });

    describe("#_popupStateChange", () => {
      it("should collapse the notification and send dismiss telemetry on 'dismissed'", () => {
        pageAction._expand();

        sandbox.spy(pageAction, "_sendTelemetry");

        pageAction._popupStateChange("dismissed");
        assert.equal(
          pageAction.urlbarinput.getAttribute("cfr-recommendation-state"),
          "collapsed"
        );

        assert.equal(
          pageAction._sendTelemetry.lastCall.args[0].event,
          "DISMISS"
        );
      });
      it("should remove the notification on 'removed'", () => {
        pageAction._expand();
        const fakeNotification = {};

        pageAction.currentNotification = fakeNotification;
        pageAction._popupStateChange("removed");
        assert.calledOnce(global.PopupNotifications.remove);
        assert.calledWith(global.PopupNotifications.remove, fakeNotification);
      });
      it("should do nothing for other states", () => {
        pageAction._popupStateChange("opened");
        assert.notCalled(global.PopupNotifications.remove);
      });
    });

    describe("#dispatchUserAction", () => {
      it("should call ._dispatchCFRAction with the right action", () => {
        const fakeAction = {};
        pageAction.dispatchUserAction(fakeAction);
        assert.calledOnce(dispatchStub);
        assert.calledWith(
          dispatchStub,
          { type: "USER_ACTION", data: fakeAction },
          fakeBrowser
        );
      });
    });

    describe("#_dispatchImpression", () => {
      it("should call ._dispatchCFRAction with the right action", () => {
        pageAction._dispatchImpression("fake impression");
        assert.calledWith(dispatchStub, {
          type: "IMPRESSION",
          data: "fake impression",
        });
      });
    });

    describe("#_sendTelemetry", () => {
      it("should call ._dispatchCFRAction with the right action", () => {
        const fakePing = { message_id: 42 };
        pageAction._sendTelemetry(fakePing);
        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {
            action: "cfr_user_event",
            source: "CFR",
            message_id: 42,
          },
        });
      });
    });

    describe("#_blockMessage", () => {
      it("should call ._dispatchCFRAction with the right action", () => {
        pageAction._blockMessage("fake id");
        assert.calledOnce(dispatchStub);
        assert.calledWith(dispatchStub, {
          type: "BLOCK_MESSAGE_BY_ID",
          data: { id: "fake id" },
        });
      });
    });

    describe("#getStrings", () => {
      let formatMessagesStub;
      const localeStrings = [
        {
          value: "你好世界",
          attributes: [
            { name: "first_attr", value: 42 },
            { name: "second_attr", value: "some string" },
            { name: "third_attr", value: [1, 2, 3] },
          ],
        },
      ];

      beforeEach(() => {
        formatMessagesStub = sandbox
          .stub()
          .withArgs({ id: "hello_world" })
          .resolves(localeStrings);
        global.RemoteL10n.l10n.formatMessages = formatMessagesStub;
      });

      it("should return the argument if a string_id is not defined", async () => {
        assert.deepEqual(await pageAction.getStrings({}), {});
        assert.equal(await pageAction.getStrings("some string"), "some string");
      });
      it("should get the right locale string", async () => {
        assert.equal(
          await pageAction.getStrings({ string_id: "hello_world" }),
          localeStrings[0].value
        );
      });
      it("should return the right sub-attribute if specified", async () => {
        assert.equal(
          await pageAction.getStrings(
            { string_id: "hello_world" },
            "second_attr"
          ),
          "some string"
        );
      });
      it("should attach attributes to string overrides", async () => {
        const fromJson = { value: "Add Now", attributes: { accesskey: "A" } };

        const result = await pageAction.getStrings(fromJson);

        assert.equal(result, fromJson.value);
        assert.propertyVal(result.attributes, "accesskey", "A");
      });
      it("should return subAttributes when doing string overrides", async () => {
        const fromJson = { value: "Add Now", attributes: { accesskey: "A" } };

        const result = await pageAction.getStrings(fromJson, "accesskey");

        assert.equal(result, "A");
      });
      it("should resolve ftl strings and attach subAttributes", async () => {
        const fromFtl = { string_id: "cfr-doorhanger-extension-ok-button" };
        formatMessagesStub.resolves([
          { value: "Add Now", attributes: [{ name: "accesskey", value: "A" }] },
        ]);

        const result = await pageAction.getStrings(fromFtl);

        assert.equal(result, "Add Now");
        assert.propertyVal(result.attributes, "accesskey", "A");
      });
      it("should return subAttributes from ftl ids", async () => {
        const fromFtl = { string_id: "cfr-doorhanger-extension-ok-button" };
        formatMessagesStub.resolves([
          { value: "Add Now", attributes: [{ name: "accesskey", value: "A" }] },
        ]);

        const result = await pageAction.getStrings(fromFtl, "accesskey");

        assert.equal(result, "A");
      });
      it("should report an error when no attributes are present but subAttribute is requested", async () => {
        const fromJson = { value: "Foo" };
        const stub = sandbox.stub(global.console, "error");

        await pageAction.getStrings(fromJson, "accesskey");

        assert.calledOnce(stub);
        stub.restore();
      });
    });

    describe("#_cfrUrlbarButtonClick", () => {
      let translateElementsStub;
      let setAttributesStub;
      let getStringsStub;
      beforeEach(async () => {
        CFRPageActions.PageActionMap.set(fakeBrowser.ownerGlobal, pageAction);
        await CFRPageActions.addRecommendation(
          fakeBrowser,
          fakeHost,
          fakeRecommendation,
          dispatchStub
        );
        getStringsStub = sandbox.stub(pageAction, "getStrings").resolves("");
        getStringsStub
          .callsFake(async a => a) // eslint-disable-line max-nested-callbacks
          .withArgs({ string_id: "primary_button_id" })
          .resolves({ value: "Primary Button", attributes: { accesskey: "p" } })
          .withArgs({ string_id: "secondary_button_id" })
          .resolves({
            value: "Secondary Button",
            attributes: { accesskey: "s" },
          })
          .withArgs({ string_id: "secondary_button_id_2" })
          .resolves({
            value: "Secondary Button 2",
            attributes: { accesskey: "a" },
          })
          .withArgs({ string_id: "secondary_button_id_3" })
          .resolves({
            value: "Secondary Button 3",
            attributes: { accesskey: "g" },
          })
          .withArgs(
            sinon.match({
              string_id: "cfr-doorhanger-extension-learn-more-link",
            })
          )
          .resolves("Learn more")
          .withArgs(
            sinon.match({ string_id: "cfr-doorhanger-extension-total-users" })
          )
          .callsFake(async ({ args }) => `${args.total} users`); // eslint-disable-line max-nested-callbacks

        translateElementsStub = sandbox.stub().resolves();
        setAttributesStub = sandbox.stub();
        global.RemoteL10n.l10n.setAttributes = setAttributesStub;
        global.RemoteL10n.l10n.translateElements = translateElementsStub;
      });

      it("should call `.hideAddressBarNotifier` and do nothing if there is no recommendation for the selected browser", async () => {
        sandbox.spy(pageAction, "hideAddressBarNotifier");
        CFRPageActions.RecommendationMap.delete(fakeBrowser);
        await pageAction._cfrUrlbarButtonClick({});
        assert.calledOnce(pageAction.hideAddressBarNotifier);
        assert.notCalled(global.PopupNotifications.show);
      });
      it("should cancel any planned state changes", async () => {
        sandbox.spy(pageAction, "_clearScheduledStateChanges");
        assert.notCalled(pageAction._clearScheduledStateChanges);
        await pageAction._cfrUrlbarButtonClick({});
        assert.calledOnce(pageAction._clearScheduledStateChanges);
      });
      it("should set the right text values", async () => {
        await pageAction._cfrUrlbarButtonClick({});
        const headerLabel = elements["cfr-notification-header-label"];
        const headerLink = elements["cfr-notification-header-link"];
        const headerImage = elements["cfr-notification-header-image"];
        const footerLink = elements["cfr-notification-footer-learn-more-link"];
        assert.equal(
          headerLabel.value,
          fakeRecommendation.content.heading_text
        );
        assert.isTrue(
          headerLink
            .getAttribute("href")
            .endsWith(fakeRecommendation.content.info_icon.sumo_path)
        );
        assert.equal(
          headerImage.getAttribute("tooltiptext"),
          fakeRecommendation.content.info_icon.label
        );
        const htmlFooterEl = fakeRemoteL10n.createElement.args.find(
          /* eslint-disable-next-line max-nested-callbacks */
          ([doc, el, args]) =>
            args && args.content === fakeRecommendation.content.text
        );
        assert.ok(htmlFooterEl);
        assert.equal(footerLink.value, "Learn more");
        assert.equal(
          footerLink.getAttribute("href"),
          fakeRecommendation.content.addon.amo_url
        );
      });
      it("should add the rating correctly", async () => {
        await pageAction._cfrUrlbarButtonClick();
        const footerFilledStars =
          elements["cfr-notification-footer-filled-stars"];
        const footerEmptyStars =
          elements["cfr-notification-footer-empty-stars"];
        // .toFixed to sort out some floating precision errors
        assert.equal(
          footerFilledStars.style.width,
          `${(4.2 * 16).toFixed(1)}px`
        );
        assert.equal(
          footerEmptyStars.style.width,
          `${(0.8 * 16).toFixed(1)}px`
        );
      });
      it("should add the number of users correctly", async () => {
        await pageAction._cfrUrlbarButtonClick();
        const footerUsers = elements["cfr-notification-footer-users"];
        assert.isNull(footerUsers.getAttribute("hidden"));
        assert.equal(
          footerUsers.getAttribute("value"),
          `${fakeRecommendation.content.addon.users}`
        );
      });
      it("should send the right telemetry", async () => {
        await pageAction._cfrUrlbarButtonClick();
        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {
            action: "cfr_user_event",
            source: "CFR",
            message_id: fakeRecommendation.id,
            bucket_id: fakeRecommendation.content.bucket_id,
            event: "CLICK_DOORHANGER",
          },
        });
      });
      it("should set the main action correctly", async () => {
        sinon
          .stub(CFRPageActions, "_fetchLatestAddonVersion")
          .resolves("latest-addon.xpi");
        await pageAction._cfrUrlbarButtonClick();
        const mainAction = global.PopupNotifications.show.firstCall.args[4]; // eslint-disable-line prefer-destructuring
        assert.deepEqual(mainAction.label, {
          value: "Primary Button",
          attributes: { accesskey: "p" },
        });
        sandbox.spy(pageAction, "hideAddressBarNotifier");
        await mainAction.callback();
        assert.calledOnce(pageAction.hideAddressBarNotifier);
        // Should block the message
        assert.calledWith(dispatchStub, {
          type: "BLOCK_MESSAGE_BY_ID",
          data: { id: fakeRecommendation.id },
        });
        // Should trigger the action
        assert.calledWith(
          dispatchStub,
          {
            type: "USER_ACTION",
            data: { id: "primary_action", data: { url: "latest-addon.xpi" } },
          },
          fakeBrowser
        );
        // Should send telemetry
        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {
            action: "cfr_user_event",
            source: "CFR",
            message_id: fakeRecommendation.id,
            bucket_id: fakeRecommendation.content.bucket_id,
            event: "INSTALL",
          },
        });
        // Should remove the recommendation
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
      it("should set the secondary action correctly", async () => {
        await pageAction._cfrUrlbarButtonClick();
        // eslint-disable-next-line prefer-destructuring
        const [secondaryAction] =
          global.PopupNotifications.show.firstCall.args[5];

        assert.deepEqual(secondaryAction.label, {
          value: "Secondary Button",
          attributes: { accesskey: "s" },
        });
        sandbox.spy(pageAction, "hideAddressBarNotifier");
        CFRPageActions.RecommendationMap.set(fakeBrowser, {});
        secondaryAction.callback();
        // Should send telemetry
        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {
            action: "cfr_user_event",
            source: "CFR",
            message_id: fakeRecommendation.id,
            bucket_id: fakeRecommendation.content.bucket_id,
            event: "DISMISS",
          },
        });
        // Don't remove the recommendation on `DISMISS` action
        assert.isTrue(CFRPageActions.RecommendationMap.has(fakeBrowser));
        assert.notCalled(pageAction.hideAddressBarNotifier);
      });
      it("should send right telemetry for BLOCK secondary action", async () => {
        await pageAction._cfrUrlbarButtonClick();
        // eslint-disable-next-line prefer-destructuring
        const blockAction = global.PopupNotifications.show.firstCall.args[5][1];

        assert.deepEqual(blockAction.label, {
          value: "Secondary Button 2",
          attributes: { accesskey: "a" },
        });
        sandbox.spy(pageAction, "hideAddressBarNotifier");
        sandbox.spy(pageAction, "_blockMessage");
        CFRPageActions.RecommendationMap.set(fakeBrowser, {});
        blockAction.callback();
        assert.calledOnce(pageAction.hideAddressBarNotifier);
        assert.calledOnce(pageAction._blockMessage);
        // Should send telemetry
        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {
            action: "cfr_user_event",
            source: "CFR",
            message_id: fakeRecommendation.id,
            bucket_id: fakeRecommendation.content.bucket_id,
            event: "BLOCK",
          },
        });
        // Should remove the recommendation
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
      it("should send right telemetry for MANAGE secondary action", async () => {
        await pageAction._cfrUrlbarButtonClick();
        // eslint-disable-next-line prefer-destructuring
        const manageAction =
          global.PopupNotifications.show.firstCall.args[5][2];

        assert.deepEqual(manageAction.label, {
          value: "Secondary Button 3",
          attributes: { accesskey: "g" },
        });
        sandbox.spy(pageAction, "hideAddressBarNotifier");
        CFRPageActions.RecommendationMap.set(fakeBrowser, {});
        manageAction.callback();
        // Should send telemetry
        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {
            action: "cfr_user_event",
            source: "CFR",
            message_id: fakeRecommendation.id,
            bucket_id: fakeRecommendation.content.bucket_id,
            event: "MANAGE",
          },
        });
        // Don't remove the recommendation on `MANAGE` action
        assert.isTrue(CFRPageActions.RecommendationMap.has(fakeBrowser));
        assert.notCalled(pageAction.hideAddressBarNotifier);
      });
      it("should call PopupNotifications.show with the right arguments", async () => {
        await pageAction._cfrUrlbarButtonClick();
        assert.calledWith(
          global.PopupNotifications.show,
          fakeBrowser,
          "contextual-feature-recommendation",
          fakeRecommendation.content.addon.title,
          "cfr",
          sinon.match.any, // Corresponds to the main action, tested above
          sinon.match.any, // Corresponds to the secondary action, tested above
          {
            popupIconURL: fakeRecommendation.content.addon.icon,
            hideClose: true,
            eventCallback: pageAction._popupStateChange,
            persistent: false,
            persistWhileVisible: false,
            popupIconClass: fakeRecommendation.content.icon_class,
            recordTelemetryInPrivateBrowsing:
              fakeRecommendation.content.show_in_private_browsing,
            name: {
              string_id: "cfr-doorhanger-extension-author",
              args: { name: fakeRecommendation.content.addon.author },
            },
          }
        );
      });
    });
    describe("#_cfrUrlbarButtonClick/cfr_urlbar_chiclet", () => {
      let heartbeatRecommendation;
      beforeEach(async () => {
        heartbeatRecommendation = (await CFRMessageProvider.getMessages()).find(
          m => m.template === "cfr_urlbar_chiclet"
        );
        CFRPageActions.PageActionMap.set(fakeBrowser.ownerGlobal, pageAction);
        await CFRPageActions.addRecommendation(
          fakeBrowser,
          fakeHost,
          heartbeatRecommendation,
          dispatchStub
        );
      });
      it("should dispatch a click event", async () => {
        await pageAction._cfrUrlbarButtonClick({});

        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {
            action: "cfr_user_event",
            source: "CFR",
            message_id: heartbeatRecommendation.id,
            bucket_id: heartbeatRecommendation.content.bucket_id,
            event: "CLICK_DOORHANGER",
          },
        });
      });
      it("should dispatch a USER_ACTION for chiclet_open_url layout", async () => {
        await pageAction._cfrUrlbarButtonClick({});

        assert.calledWith(dispatchStub, {
          type: "USER_ACTION",
          data: {
            data: {
              args: heartbeatRecommendation.content.action.url,
              where: heartbeatRecommendation.content.action.where,
            },
            type: "OPEN_URL",
          },
        });
      });
      it("should block the message after the click", async () => {
        await pageAction._cfrUrlbarButtonClick({});

        assert.calledWith(dispatchStub, {
          type: "BLOCK_MESSAGE_BY_ID",
          data: { id: heartbeatRecommendation.id },
        });
      });
      it("should remove the button and browser entry", async () => {
        sandbox.spy(pageAction, "hideAddressBarNotifier");

        await pageAction._cfrUrlbarButtonClick({});

        assert.calledOnce(pageAction.hideAddressBarNotifier);
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
    });
  });

  describe("CFRPageActions", () => {
    beforeEach(() => {
      // Spy on the prototype methods to inspect calls for any PageAction instance
      sandbox.spy(PageAction.prototype, "showAddressBarNotifier");
      sandbox.spy(PageAction.prototype, "hideAddressBarNotifier");
    });

    describe("updatePageActions", () => {
      let savedRec;

      beforeEach(() => {
        const win = fakeBrowser.ownerGlobal;
        CFRPageActions.PageActionMap.set(
          win,
          new PageAction(win, dispatchStub)
        );
        const { id, content } = fakeRecommendation;
        savedRec = {
          id,
          host: fakeHost,
          content,
        };
        CFRPageActions.RecommendationMap.set(fakeBrowser, savedRec);
      });

      it("should do nothing if a pageAction doesn't exist for the window", () => {
        const win = fakeBrowser.ownerGlobal;
        CFRPageActions.PageActionMap.delete(win);
        CFRPageActions.updatePageActions(fakeBrowser);
        assert.notCalled(PageAction.prototype.showAddressBarNotifier);
        assert.notCalled(PageAction.prototype.hideAddressBarNotifier);
      });
      it("should do nothing if the browser is not the `selectedBrowser`", () => {
        const someOtherFakeBrowser = {};
        CFRPageActions.updatePageActions(someOtherFakeBrowser);
        assert.notCalled(PageAction.prototype.showAddressBarNotifier);
        assert.notCalled(PageAction.prototype.hideAddressBarNotifier);
      });
      it("should hideAddressBarNotifier the pageAction if a recommendation doesn't exist for the given browser", () => {
        CFRPageActions.RecommendationMap.delete(fakeBrowser);
        CFRPageActions.updatePageActions(fakeBrowser);
        assert.calledOnce(PageAction.prototype.hideAddressBarNotifier);
      });
      it("should show the pageAction if a recommendation exists and the host matches", () => {
        CFRPageActions.updatePageActions(fakeBrowser);
        assert.calledOnce(PageAction.prototype.showAddressBarNotifier);
        assert.calledWith(
          PageAction.prototype.showAddressBarNotifier,
          savedRec
        );
      });
      it("should show the pageAction if a recommendation exists and it doesn't have a host defined", () => {
        const recNoHost = { ...savedRec, host: undefined };
        CFRPageActions.RecommendationMap.set(fakeBrowser, recNoHost);
        CFRPageActions.updatePageActions(fakeBrowser);
        assert.calledOnce(PageAction.prototype.showAddressBarNotifier);
        assert.calledWith(
          PageAction.prototype.showAddressBarNotifier,
          recNoHost
        );
      });
      it("should hideAddressBarNotifier the pageAction and delete the recommendation if the recommendation exists but the host doesn't match", () => {
        const someOtherFakeHost = "subdomain.mozilla.com";
        fakeBrowser.documentURI.host = someOtherFakeHost;
        assert.isTrue(CFRPageActions.RecommendationMap.has(fakeBrowser));
        CFRPageActions.updatePageActions(fakeBrowser);
        assert.calledOnce(PageAction.prototype.hideAddressBarNotifier);
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
      it("should not call `delete` if retain is true", () => {
        savedRec.retain = true;
        fakeBrowser.documentURI.host = "subdomain.mozilla.com";
        assert.isTrue(CFRPageActions.RecommendationMap.has(fakeBrowser));

        CFRPageActions.updatePageActions(fakeBrowser);
        assert.propertyVal(savedRec, "retain", false);
        assert.calledOnce(PageAction.prototype.hideAddressBarNotifier);
        assert.isTrue(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
      it("should call `delete` if retain is false", () => {
        savedRec.retain = false;
        fakeBrowser.documentURI.host = "subdomain.mozilla.com";
        assert.isTrue(CFRPageActions.RecommendationMap.has(fakeBrowser));

        CFRPageActions.updatePageActions(fakeBrowser);
        assert.propertyVal(savedRec, "retain", false);
        assert.calledOnce(PageAction.prototype.hideAddressBarNotifier);
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
    });

    describe("forceRecommendation", () => {
      it("should succeed and add an element to the RecommendationMap", async () => {
        assert.isTrue(
          await CFRPageActions.forceRecommendation(
            fakeBrowser,
            fakeRecommendation,
            dispatchStub
          )
        );
        assert.deepInclude(CFRPageActions.RecommendationMap.get(fakeBrowser), {
          id: fakeRecommendation.id,
          content: fakeRecommendation.content,
        });
      });
      it("should create a PageAction if one doesn't exist for the window, save it in the PageActionMap, and call `show`", async () => {
        const win = fakeBrowser.ownerGlobal;
        assert.isFalse(CFRPageActions.PageActionMap.has(win));
        await CFRPageActions.forceRecommendation(
          fakeBrowser,
          fakeRecommendation,
          dispatchStub
        );
        const pageAction = CFRPageActions.PageActionMap.get(win);
        assert.equal(win, pageAction.window);
        assert.equal(dispatchStub, pageAction._dispatchCFRAction);
        assert.calledOnce(PageAction.prototype.showAddressBarNotifier);
      });
    });

    describe("showPopup", () => {
      let savedRec;
      let pageAction;
      let fakeAnchorId = "fake_anchor_id";
      let fakeAltAnchorId = "fake_alt_anchor_id";
      let TEST_MESSAGE;
      let getElmStub;
      let getStyleStub;
      let isElmVisibleStub;
      let getWidgetStub;
      let isCustomizingStub;
      beforeEach(() => {
        TEST_MESSAGE = {
          id: "fake_id",
          template: "cfr_doorhanger",
          content: {
            skip_address_bar_notifier: true,
            heading_text: "Fake Heading Text",
            anchor_id: fakeAnchorId,
          },
        };
        getElmStub = sandbox
          .stub(window.document, "getElementById")
          .callsFake(id => ({ id }));
        getStyleStub = sandbox
          .stub(window, "getComputedStyle")
          .returns({ display: "block", visibility: "visible" });
        isElmVisibleStub = sandbox.stub().returns(true);
        getWidgetStub = sandbox.stub();
        isCustomizingStub = sandbox.stub().returns(false);
        globals.set({
          CustomizableUI: { getWidget: getWidgetStub },
          CustomizationHandler: { isCustomizing: isCustomizingStub },
          isElementVisible: isElmVisibleStub,
        });

        savedRec = {
          id: TEST_MESSAGE.id,
          host: fakeHost,
          content: TEST_MESSAGE.content,
        };
        CFRPageActions.RecommendationMap.set(fakeBrowser, savedRec);
        pageAction = new PageAction(window, dispatchStub);
        sandbox.stub(pageAction, "_renderPopup");
      });
      afterEach(() => {
        sandbox.restore();
        globals.restore();
      });

      it("should use anchor_id if element exists and is not a customizable widget", async () => {
        await pageAction.showPopup();
        assert.equal(fakeBrowser.cfrpopupnotificationanchor.id, fakeAnchorId);
      });

      it("should use anchor_id if element exists and is in the toolbar", async () => {
        getWidgetStub.withArgs(fakeAnchorId).returns({ areaType: "toolbar" });
        await pageAction.showPopup();
        assert.equal(fakeBrowser.cfrpopupnotificationanchor.id, fakeAnchorId);
      });

      it("should use default container if element exists but is in the widget overflow panel", async () => {
        getWidgetStub
          .withArgs(fakeAnchorId)
          .returns({ areaType: "menu-panel" });
        await pageAction.showPopup();
        assert.equal(
          fakeBrowser.cfrpopupnotificationanchor.id,
          pageAction.container.id
        );
      });

      it("should use default container if element exists but is in the customization palette", async () => {
        getWidgetStub.withArgs(fakeAnchorId).returns({ areaType: null });
        isCustomizingStub.returns(true);
        await pageAction.showPopup();
        assert.equal(
          fakeBrowser.cfrpopupnotificationanchor.id,
          pageAction.container.id
        );
      });

      it("should use alt_anchor_id if one has been provided and the anchor_id element cannot be found", async () => {
        TEST_MESSAGE.content.alt_anchor_id = fakeAltAnchorId;
        getElmStub.withArgs(fakeAnchorId).returns(null);
        await pageAction.showPopup();
        assert.equal(
          fakeBrowser.cfrpopupnotificationanchor.id,
          fakeAltAnchorId
        );
      });

      it("should use alt_anchor_id if one has been provided and the anchor_id element is hidden by CSS", async () => {
        TEST_MESSAGE.content.alt_anchor_id = fakeAltAnchorId;
        getStyleStub
          .withArgs(sandbox.match({ id: fakeAnchorId }))
          .returns({ display: "none", visibility: "visible" });
        await pageAction.showPopup();
        assert.equal(
          fakeBrowser.cfrpopupnotificationanchor.id,
          fakeAltAnchorId
        );
      });

      it("should use alt_anchor_id if one has been provided and the anchor_id element has no height/width", async () => {
        TEST_MESSAGE.content.alt_anchor_id = fakeAltAnchorId;
        isElmVisibleStub
          .withArgs(sandbox.match({ id: fakeAnchorId }))
          .returns(false);
        await pageAction.showPopup();
        assert.equal(
          fakeBrowser.cfrpopupnotificationanchor.id,
          fakeAltAnchorId
        );
      });

      it("should use default container if the anchor_id and alt_anchor_id are both not visible", async () => {
        TEST_MESSAGE.content.alt_anchor_id = fakeAltAnchorId;
        getStyleStub
          .withArgs(sandbox.match({ id: fakeAnchorId }))
          .returns({ display: "none", visibility: "visible" });
        getWidgetStub.withArgs(fakeAltAnchorId).returns({ areaType: null });
        isCustomizingStub.returns(true);
        await pageAction.showPopup();
        assert.equal(
          fakeBrowser.cfrpopupnotificationanchor.id,
          pageAction.container.id
        );
      });
    });

    describe("addRecommendation", () => {
      it("should fail and not add a recommendation if the browser is part of a private window", async () => {
        global.PrivateBrowsingUtils.isWindowPrivate.returns(true);
        assert.isFalse(
          await CFRPageActions.addRecommendation(
            fakeBrowser,
            fakeHost,
            fakeRecommendation,
            dispatchStub
          )
        );
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
      it("should successfully add a private browsing recommendation and send correct telemetry", async () => {
        global.PrivateBrowsingUtils.isWindowPrivate.returns(true);
        fakeRecommendation.content.show_in_private_browsing = true;
        assert.isTrue(
          await CFRPageActions.addRecommendation(
            fakeBrowser,
            fakeHost,
            fakeRecommendation,
            dispatchStub
          )
        );
        assert.isTrue(CFRPageActions.RecommendationMap.has(fakeBrowser));

        const pageAction = CFRPageActions.PageActionMap.get(
          fakeBrowser.ownerGlobal
        );
        await pageAction.showAddressBarNotifier(fakeRecommendation, true);
        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {
            action: "cfr_user_event",
            source: "CFR",
            is_private: true,
            message_id: fakeRecommendation.id,
            bucket_id: fakeRecommendation.content.bucket_id,
            event: "IMPRESSION",
          },
        });
      });
      it("should fail and not add a recommendation if the browser is not the selected browser", async () => {
        global.gBrowser.selectedBrowser = {}; // Some other browser
        assert.isFalse(
          await CFRPageActions.addRecommendation(
            fakeBrowser,
            fakeHost,
            fakeRecommendation,
            dispatchStub
          )
        );
      });
      it("should fail and not add a recommendation if the browser does not exist", async () => {
        assert.isFalse(
          await CFRPageActions.addRecommendation(
            undefined,
            fakeHost,
            fakeRecommendation,
            dispatchStub
          )
        );
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
      it("should fail and not add a recommendation if the host doesn't match", async () => {
        const someOtherFakeHost = "subdomain.mozilla.com";
        assert.isFalse(
          await CFRPageActions.addRecommendation(
            fakeBrowser,
            someOtherFakeHost,
            fakeRecommendation,
            dispatchStub
          )
        );
      });
      it("should otherwise succeed and add an element to the RecommendationMap", async () => {
        assert.isTrue(
          await CFRPageActions.addRecommendation(
            fakeBrowser,
            fakeHost,
            fakeRecommendation,
            dispatchStub
          )
        );
        assert.deepInclude(CFRPageActions.RecommendationMap.get(fakeBrowser), {
          id: fakeRecommendation.id,
          host: fakeHost,
          content: fakeRecommendation.content,
        });
      });
      it("should create a PageAction if one doesn't exist for the window, save it in the PageActionMap, and call `show`", async () => {
        const win = fakeBrowser.ownerGlobal;
        assert.isFalse(CFRPageActions.PageActionMap.has(win));
        await CFRPageActions.addRecommendation(
          fakeBrowser,
          fakeHost,
          fakeRecommendation,
          dispatchStub
        );
        const pageAction = CFRPageActions.PageActionMap.get(win);
        assert.equal(win, pageAction.window);
        assert.equal(dispatchStub, pageAction._dispatchCFRAction);
        assert.calledOnce(PageAction.prototype.showAddressBarNotifier);
      });
      it("should add the right url if we fetched and addon install URL", async () => {
        fakeRecommendation.template = "cfr_doorhanger";
        await CFRPageActions.addRecommendation(
          fakeBrowser,
          fakeHost,
          fakeRecommendation,
          dispatchStub
        );
        const recommendation =
          CFRPageActions.RecommendationMap.get(fakeBrowser);

        // sanity check - just go through some of the rest of the attributes to make sure they were untouched
        assert.equal(recommendation.id, fakeRecommendation.id);
        assert.equal(
          recommendation.content.heading_text,
          fakeRecommendation.content.heading_text
        );
        assert.equal(
          recommendation.content.addon,
          fakeRecommendation.content.addon
        );
        assert.equal(
          recommendation.content.text,
          fakeRecommendation.content.text
        );
        assert.equal(
          recommendation.content.buttons.secondary,
          fakeRecommendation.content.buttons.secondary
        );
        assert.equal(
          recommendation.content.buttons.primary.action.id,
          fakeRecommendation.content.buttons.primary.action.id
        );

        delete fakeRecommendation.template;
      });
      it("should prevent a second message if one is currently displayed", async () => {
        const secondMessage = { ...fakeRecommendation, id: "second_message" };
        let messageAdded = await CFRPageActions.addRecommendation(
          fakeBrowser,
          fakeHost,
          fakeRecommendation,
          dispatchStub
        );

        assert.isTrue(messageAdded);
        assert.deepInclude(CFRPageActions.RecommendationMap.get(fakeBrowser), {
          id: fakeRecommendation.id,
          host: fakeHost,
          content: fakeRecommendation.content,
        });

        messageAdded = await CFRPageActions.addRecommendation(
          fakeBrowser,
          fakeHost,
          secondMessage,
          dispatchStub
        );
        // Adding failed
        assert.isFalse(messageAdded);
        // First message is still there
        assert.deepInclude(CFRPageActions.RecommendationMap.get(fakeBrowser), {
          id: fakeRecommendation.id,
          host: fakeHost,
          content: fakeRecommendation.content,
        });
      });
      it("should send impressions just for the first message", async () => {
        const secondMessage = { ...fakeRecommendation, id: "second_message" };
        await CFRPageActions.addRecommendation(
          fakeBrowser,
          fakeHost,
          fakeRecommendation,
          dispatchStub
        );
        await CFRPageActions.addRecommendation(
          fakeBrowser,
          fakeHost,
          secondMessage,
          dispatchStub
        );

        // Doorhanger telemetry + Impression for just 1 message
        assert.calledTwice(dispatchStub);
        const [firstArgs] = dispatchStub.firstCall.args;
        const [secondArgs] = dispatchStub.secondCall.args;
        assert.equal(firstArgs.data.id, secondArgs.data.message_id);
      });
    });

    describe("clearRecommendations", () => {
      const createFakePageAction = () => ({
        hideAddressBarNotifier: sandbox.stub(),
      });
      const windows = [{}, {}, { closed: true }];
      const browsers = [{}, {}, {}, {}];

      beforeEach(() => {
        CFRPageActions.PageActionMap.set(windows[0], createFakePageAction());
        CFRPageActions.PageActionMap.set(windows[2], createFakePageAction());
        for (const browser of browsers) {
          CFRPageActions.RecommendationMap.set(browser, {});
        }
        globals.set({ Services: { wm: { getEnumerator: () => windows } } });
      });

      it("should hideAddressBarNotifier the PageActions of any existing, non-closed windows", () => {
        const pageActions = windows.map(win =>
          CFRPageActions.PageActionMap.get(win)
        );
        CFRPageActions.clearRecommendations();

        // Only the first window had a PageAction and wasn't closed
        assert.calledOnce(pageActions[0].hideAddressBarNotifier);
        assert.isUndefined(pageActions[1]);
        assert.notCalled(pageActions[2].hideAddressBarNotifier);
      });
      it("should clear the PageActionMap and the RecommendationMap", () => {
        CFRPageActions.clearRecommendations();

        // Both are WeakMaps and so are not iterable, cannot be cleared, and
        // cannot have their length queried directly, so we have to check
        // whether previous elements still exist
        assert.lengthOf(windows, 3);
        for (const win of windows) {
          assert.isFalse(CFRPageActions.PageActionMap.has(win));
        }
        assert.lengthOf(browsers, 4);
        for (const browser of browsers) {
          assert.isFalse(CFRPageActions.RecommendationMap.has(browser));
        }
      });
    });

    describe("reloadL10n", () => {
      const createFakePageAction = () => ({
        hideAddressBarNotifier() {},
        reloadL10n: sandbox.stub(),
      });
      const windows = [{}, {}, { closed: true }];

      beforeEach(() => {
        CFRPageActions.PageActionMap.set(windows[0], createFakePageAction());
        CFRPageActions.PageActionMap.set(windows[2], createFakePageAction());
        globals.set({ Services: { wm: { getEnumerator: () => windows } } });
      });

      it("should call reloadL10n for all the PageActions of any existing, non-closed windows", () => {
        const pageActions = windows.map(win =>
          CFRPageActions.PageActionMap.get(win)
        );
        CFRPageActions.reloadL10n();

        // Only the first window had a PageAction and wasn't closed
        assert.calledOnce(pageActions[0].reloadL10n);
        assert.isUndefined(pageActions[1]);
        assert.notCalled(pageActions[2].reloadL10n);
      });
    });
  });
});

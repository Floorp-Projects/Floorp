import {CFRPageActions, PageAction} from "lib/CFRPageActions.jsm";
import {GlobalOverrider} from "test/unit/utils";

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

  const elementIDs = [
    "urlbar",
    "contextual-feature-recommendation",
    "cfr-button",
    "cfr-label",
    "contextual-feature-recommendation-notification",
    "cfr-notification-header-label",
    "cfr-notification-header-link",
    "cfr-notification-header-image",
    "cfr-notification-author",
    "cfr-notification-footer-text",
    "cfr-notification-footer-filled-stars",
    "cfr-notification-footer-empty-stars",
    "cfr-notification-footer-users",
    "cfr-notification-footer-spacer",
    "cfr-notification-footer-learn-more-link",
  ];

  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    clock = sandbox.useFakeTimers();

    fakeRecommendation = {
      id: "fake_id",
      content: {
        bucket_id: "fake_bucket_id",
        notification_text: "Fake Notification Text",
        info_icon: {
          label: "Fake Info Icon Label",
          sumo_path: "a_help_path_fragment",
        },
        heading_text: "Fake Heading Text",
        addon:  {
          title: "Fake Addon Title",
          author: "Fake Addon Author",
          icon: "a_path_to_some_icon",
          rating: 4.2,
          users: 1234,
          amo_url: "a_path_to_amo",
        },
        text: "Here is the recommendation text body",
        buttons: {
          primary: {
            label: {string_id: "primary_button_id"},
            action: {id: "primary_action"},
          },
          secondary: [{
            label: {string_id: "secondary_button_id"},
            action: {id: "secondary_action"},
          }, {
            label: {string_id: "secondary_button_id_2"},
            action: {id: "secondary_action"},
          }, {
            label: {string_id: "secondary_button_id_3"},
            action: {id: "secondary_action"},
          }],
        },
      },
    };
    fakeHost = "mozilla.org";
    fakeBrowser = {
      documentURI: {
        scheme: "https",
        host: fakeHost,
      },
      ownerGlobal: window,
    };
    dispatchStub = sandbox.stub();

    globals = new GlobalOverrider();
    globals.set({
      Localization: class {},
      promiseDocumentFlushed: Promise.resolve(),
      PopupNotifications: {
        show: sandbox.stub(),
        remove: sandbox.stub(),
      },
      PrivateBrowsingUtils: {isWindowPrivate: sandbox.stub().returns(false)},
      gBrowser: {selectedBrowser: fakeBrowser},
    });

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
  });

  afterEach(() => {
    CFRPageActions.clearRecommendations();
    containerElem.remove();
    sandbox.restore();
    globals.restore();
  });

  describe("PageAction", () => {
    let pageAction;
    let getStringsStub;

    beforeEach(() => {
      pageAction = new PageAction(window, dispatchStub);
      getStringsStub = sandbox.stub(pageAction, "getStrings").resolves("");
    });

    describe("#show", () => {
      it("should un-hide the element and set the right label value", async () => {
        const FAKE_NOTIFICATION_TEXT = "FAKE_NOTIFICATION_TEXT";
        getStringsStub.withArgs(fakeRecommendation.content.notification_text).resolves(FAKE_NOTIFICATION_TEXT);
        await pageAction.show(fakeRecommendation);
        assert.isFalse(pageAction.container.hidden);
        assert.equal(pageAction.label.value, FAKE_NOTIFICATION_TEXT);
      });
      it("should wait for the document layout to flush", async () => {
        sandbox.spy(pageAction.label, "getClientRects");
        const promiseGetterStub = sandbox.stub().resolves();
        sandbox.stub(global, "promiseDocumentFlushed").get(promiseGetterStub);
        await pageAction.show(fakeRecommendation);
        assert.calledOnce(promiseGetterStub);
        assert.callOrder(promiseGetterStub, pageAction.label.getClientRects);
      });
      it("should set the CSS variable --cfr-label-width correctly", async () => {
        await pageAction.show(fakeRecommendation);
        const expectedWidth = pageAction.label.getClientRects()[0].width;
        assert.equal(pageAction.urlbar.style.getPropertyValue("--cfr-label-width"),
          `${expectedWidth}px`);
      });
      it("should cause an expansion, and dispatch an impression iff `expand` is true", async () => {
        sandbox.spy(pageAction, "_clearScheduledStateChanges");
        sandbox.spy(pageAction, "_expand");
        sandbox.spy(pageAction, "_dispatchImpression");

        await pageAction.show(fakeRecommendation);
        assert.notCalled(pageAction._dispatchImpression);
        clock.tick(1001);
        assert.notEqual(pageAction.urlbar.getAttribute("cfr-recommendation-state"), "expanded");

        await pageAction.show(fakeRecommendation, true);
        assert.calledOnce(pageAction._clearScheduledStateChanges);
        clock.tick(1001);
        assert.equal(pageAction.urlbar.getAttribute("cfr-recommendation-state"), "expanded");
        assert.calledOnce(pageAction._dispatchImpression);
        assert.calledWith(pageAction._dispatchImpression, fakeRecommendation);
      });
      it("should send telemetry if `expand` is true and the id and bucket_id are provided", async () => {
        await pageAction.show(fakeRecommendation, true);
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

    describe("#hide", () => {
      it("should hide the container, cancel any state changes, and remove the state attribute", () => {
        sandbox.spy(pageAction, "_clearScheduledStateChanges");
        pageAction.hide();
        assert.isTrue(pageAction.container.hidden);
        assert.calledOnce(pageAction._clearScheduledStateChanges);
        assert.isNull(pageAction.urlbar.getAttribute("cfr-recommendation-state"));
      });
      it("should remove the `currentNotification`", () => {
        const notification = {};
        pageAction.currentNotification = notification;
        pageAction.hide();
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
        assert.equal(pageAction.urlbar.getAttribute("cfr-recommendation-state"), "expanded");
      });
      it("with a delay, should set the expanded state after the correct amount of time", () => {
        const delay = 1234;
        pageAction._expand(delay);
        // We expect that an expansion has been scheduled
        assert.lengthOf(pageAction.stateTransitionTimeoutIDs, 1);
        clock.tick(delay + 1);
        assert.equal(pageAction.urlbar.getAttribute("cfr-recommendation-state"), "expanded");
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
        assert.isNull(pageAction.urlbar.getAttribute("cfr-recommendation-state"));
        pageAction.urlbar.setAttribute("cfr-recommendation-state", "expanded");
        pageAction._collapse();
        assert.equal(pageAction.urlbar.getAttribute("cfr-recommendation-state"), "collapsed");
      });
      it("with a delay, should set the collapsed state after the correct amount of time", () => {
        const delay = 1234;
        pageAction._collapse(delay);
        clock.tick(delay + 1);
        // The state was _not_ "expanded" and so should not have been set to "collapsed"
        assert.isNull(pageAction.urlbar.getAttribute("cfr-recommendation-state"));

        pageAction._expand();
        pageAction._collapse(delay);
        // We expect that a collapse has been scheduled
        assert.lengthOf(pageAction.stateTransitionTimeoutIDs, 1);
        clock.tick(delay + 1);
        // This time it was "expanded" so should now (after the delay) be "collapsed"
        assert.equal(pageAction.urlbar.getAttribute("cfr-recommendation-state"), "collapsed");
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
      it("should collapse and remove the notification on 'dismissed'", () => {
        pageAction._expand();
        const fakeNotification = {};

        pageAction.currentNotification = fakeNotification;
        pageAction._popupStateChange("dismissed");
        assert.equal(pageAction.urlbar.getAttribute("cfr-recommendation-state"), "collapsed");
        assert.calledOnce(global.PopupNotifications.remove);
        assert.calledWith(global.PopupNotifications.remove, fakeNotification);
      });
      it("should collapse and remove the notification on 'removed'", () => {
        pageAction._expand();
        const fakeNotification = {};

        pageAction.currentNotification = fakeNotification;
        pageAction._popupStateChange("removed");
        assert.equal(pageAction.urlbar.getAttribute("cfr-recommendation-state"), "collapsed");
        assert.calledOnce(global.PopupNotifications.remove);
        assert.calledWith(global.PopupNotifications.remove, fakeNotification);
      });
      it("should do nothing for other states", () => {
        pageAction._popupStateChange("opened");
        assert.notCalled(global.PopupNotifications.remove);
      });
    });

    describe("#dispatchUserAction", () => {
      it("should call ._dispatchToASRouter with the right action", () => {
        const fakeAction = {};
        pageAction.dispatchUserAction(fakeAction);
        assert.calledOnce(dispatchStub);
        assert.calledWith(
          dispatchStub,
          {type: "USER_ACTION", data: fakeAction},
          {browser: fakeBrowser}
        );
      });
    });

    describe("#_dispatchImpression", () => {
      it("should call ._dispatchToASRouter with the right action", () => {
        pageAction._dispatchImpression("fake impression");
        assert.calledWith(dispatchStub, {type: "IMPRESSION", data: "fake impression"});
      });
    });

    describe("#_sendTelemetry", () => {
      it("should call ._dispatchToASRouter with the right action", () => {
        const fakePing = {message_id: 42};
        pageAction._sendTelemetry(fakePing);
        assert.calledWith(dispatchStub, {
          type: "DOORHANGER_TELEMETRY",
          data: {action: "cfr_user_event", source: "CFR", message_id: 42},
        });
      });
    });

    describe("#_blockMessage", () => {
      it("should call ._dispatchToASRouter with the right action", () => {
        pageAction._blockMessage("fake id");
        assert.calledOnce(dispatchStub);
        assert.calledWith(dispatchStub, {
          type: "BLOCK_MESSAGE_BY_ID",
          data: {id: "fake id"},
        });
      });
    });

    describe("#getStrings", () => {
      let formatMessagesStub;
      const localeStrings = [{
        value: "你好世界",
        attributes: [
          {name: "first_attr", value: 42},
          {name: "second_attr", value: "some string"},
          {name: "third_attr", value: [1, 2, 3]},
        ],
      }];

      beforeEach(() => {
        getStringsStub.restore();
        formatMessagesStub = sandbox.stub()
          .withArgs({id: "hello_world"})
          .resolves(localeStrings);
        global.Localization.prototype.formatMessages = formatMessagesStub;
      });

      it("should return the argument if a string_id is not defined", async () => {
        assert.deepEqual(await pageAction.getStrings({}), {});
        assert.equal(await pageAction.getStrings("some string"), "some string");
      });
      it("should get the right locale string", async () => {
        assert.equal(await pageAction.getStrings({string_id: "hello_world"}), localeStrings[0].value);
      });
      it("should return the right sub-attribute if specified", async () => {
        assert.equal(await pageAction.getStrings({string_id: "hello_world"}, "second_attr"), "some string");
      });
      it("should attach attributes to string overrides", async () => {
        const fromJson = {value: "Add Now", attributes: {accesskey: "A"}};

        const result = await pageAction.getStrings(fromJson);

        assert.equal(result, fromJson.value);
        assert.propertyVal(result.attributes, "accesskey", "A");
      });
      it("should return subAttributes when doing string overrides", async () => {
        const fromJson = {value: "Add Now", attributes: {accesskey: "A"}};

        const result = await pageAction.getStrings(fromJson, "accesskey");

        assert.equal(result, "A");
      });
      it("should resolve ftl strings and attach subAttributes", async () => {
        const fromFtl = {string_id: "cfr-doorhanger-extension-ok-button"};
        formatMessagesStub.resolves([{value: "Add Now", attributes: [{name: "accesskey", value: "A"}]}]);

        const result = await pageAction.getStrings(fromFtl);

        assert.equal(result, "Add Now");
        assert.propertyVal(result.attributes, "accesskey", "A");
      });
      it("should return subAttributes from ftl ids", async () => {
        const fromFtl = {string_id: "cfr-doorhanger-extension-ok-button"};
        formatMessagesStub.resolves([{value: "Add Now", attributes: [{name: "accesskey", value: "A"}]}]);

        const result = await pageAction.getStrings(fromFtl, "accesskey");

        assert.equal(result, "A");
      });
      it("should report an error when no attributes are present but subAttribute is requested", async () => {
        const fromJson = {value: "Foo"};
        const stub = sandbox.stub(global.Cu, "reportError");

        await pageAction.getStrings(fromJson, "accesskey");

        assert.calledOnce(stub);
        stub.restore();
      });
    });

    describe("#_handleClick", () => {
      beforeEach(async () => {
        CFRPageActions.PageActionMap.set(fakeBrowser.ownerGlobal, pageAction);
        await CFRPageActions.addRecommendation(fakeBrowser, fakeHost, fakeRecommendation, dispatchStub);
        getStringsStub.callsFake(async a => a) // eslint-disable-line max-nested-callbacks
          .withArgs({string_id: "primary_button_id"})
          .resolves({value: "Primary Button", attributes: {accesskey: "p"}})
          .withArgs({string_id: "secondary_button_id"})
          .resolves({value: "Secondary Button", attributes: {accesskey: "s"}})
          .withArgs({string_id: "secondary_button_id_2"})
          .resolves({value: "Secondary Button 2", attributes: {accesskey: "a"}})
          .withArgs({string_id: "secondary_button_id_3"})
          .resolves({value: "Secondary Button 3", attributes: {accesskey: "g"}})
          .withArgs(sinon.match({string_id: "cfr-doorhanger-extension-learn-more-link"}))
          .resolves("Learn more")
          .withArgs(sinon.match({string_id: "cfr-doorhanger-extension-total-users"}))
          .callsFake(async ({args}) => `${args.total} users`); // eslint-disable-line max-nested-callbacks
      });

      it("should call `.hide` and do nothing if there is no recommendation for the selected browser", async () => {
        sandbox.spy(pageAction, "hide");
        CFRPageActions.RecommendationMap.delete(fakeBrowser);
        await pageAction._handleClick({});
        assert.calledOnce(pageAction.hide);
        assert.notCalled(global.PopupNotifications.show);
      });
      it("should cancel any planned state changes", async () => {
        sandbox.spy(pageAction, "_clearScheduledStateChanges");
        assert.notCalled(pageAction._clearScheduledStateChanges);
        await pageAction._handleClick({});
        assert.calledOnce(pageAction._clearScheduledStateChanges);
      });
      it("should set the right text values", async () => {
        await pageAction._handleClick({});
        const headerLabel = elements["cfr-notification-header-label"];
        const headerLink = elements["cfr-notification-header-link"];
        const headerImage = elements["cfr-notification-header-image"];
        const footerText = elements["cfr-notification-footer-text"];
        const footerLink = elements["cfr-notification-footer-learn-more-link"];
        assert.equal(headerLabel.value, fakeRecommendation.content.heading_text);
        assert.isTrue(headerLink.getAttribute("href").endsWith(fakeRecommendation.content.info_icon.sumo_path));
        assert.equal(headerImage.getAttribute("tooltiptext"), fakeRecommendation.content.info_icon.label);
        assert.equal(footerText.textContent, fakeRecommendation.content.text);
        assert.equal(footerLink.value, "Learn more");
        assert.equal(footerLink.getAttribute("href"), fakeRecommendation.content.addon.amo_url);
      });
      it("should add the rating correctly", async () => {
        await pageAction._handleClick();
        const footerFilledStars = elements["cfr-notification-footer-filled-stars"];
        const footerEmptyStars = elements["cfr-notification-footer-empty-stars"];
        // .toFixed to sort out some floating precision errors
        assert.equal(footerFilledStars.style.width, `${(4.2 * 17).toFixed(1)}px`);
        assert.equal(footerEmptyStars.style.width, `${(0.8 * 17).toFixed(1)}px`);
      });
      it("should add the number of users correctly", async () => {
        await pageAction._handleClick();
        const footerUsers = elements["cfr-notification-footer-users"];
        assert.isNull(footerUsers.getAttribute("hidden"));
        assert.equal(footerUsers.getAttribute("value"), `${fakeRecommendation.content.addon.users} users`);
      });
      it("should send the right telemetry", async () => {
        await pageAction._handleClick();
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
        await pageAction._handleClick();
        const mainAction = global.PopupNotifications.show.firstCall.args[4]; // eslint-disable-line prefer-destructuring

        assert.deepEqual(mainAction.label, {value: "Primary Button", attributes: {accesskey: "p"}});
        sandbox.spy(pageAction, "hide");
        mainAction.callback();
        assert.calledOnce(pageAction.hide);
        // Should block the message
        assert.calledWith(dispatchStub, {
          type: "BLOCK_MESSAGE_BY_ID",
          data: {id: fakeRecommendation.id},
        });
        // Should trigger the action
        assert.calledWith(
          dispatchStub,
          {type: "USER_ACTION", data: {id: "primary_action"}},
          {browser: fakeBrowser}
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
        await pageAction._handleClick();
        const [secondaryAction] = global.PopupNotifications.show.firstCall.args[5]; // eslint-disable-line prefer-destructuring

        assert.deepEqual(secondaryAction.label, {value: "Secondary Button", attributes: {accesskey: "s"}});
        sandbox.spy(pageAction, "hide");
        CFRPageActions.RecommendationMap.set(fakeBrowser, {});
        secondaryAction.callback();
        assert.calledOnce(pageAction.hide);
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
        // Should remove the recommendation
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
      it("should send right telemetry for BLOCK secondary action", async () => {
        await pageAction._handleClick();
        const blockAction = global.PopupNotifications.show.firstCall.args[5][1]; // eslint-disable-line prefer-destructuring

        assert.deepEqual(blockAction.label, {value: "Secondary Button 2", attributes: {accesskey: "a"}});
        sandbox.spy(pageAction, "hide");
        sandbox.spy(pageAction, "_blockMessage");
        CFRPageActions.RecommendationMap.set(fakeBrowser, {});
        blockAction.callback();
        assert.calledOnce(pageAction.hide);
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
        await pageAction._handleClick();
        const blockAction = global.PopupNotifications.show.firstCall.args[5][2]; // eslint-disable-line prefer-destructuring

        assert.deepEqual(blockAction.label, {value: "Secondary Button 3", attributes: {accesskey: "g"}});
        sandbox.spy(pageAction, "hide");
        CFRPageActions.RecommendationMap.set(fakeBrowser, {});
        blockAction.callback();
        assert.calledOnce(pageAction.hide);
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
        // Should remove the recommendation
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
      it("should call PopupNotifications.show with the right arguments", async () => {
        await pageAction._handleClick();
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
          }
        );
      });
    });
  });

  describe("CFRPageActions", () => {
    beforeEach(() => {
      // Spy on the prototype methods to inspect calls for any PageAction instance
      sandbox.spy(PageAction.prototype, "show");
      sandbox.spy(PageAction.prototype, "hide");
    });

    describe("updatePageActions", () => {
      let savedRec;

      beforeEach(() => {
        const win = fakeBrowser.ownerGlobal;
        CFRPageActions.PageActionMap.set(win, new PageAction(win, dispatchStub));
        const {id, content} = fakeRecommendation;
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
        assert.notCalled(PageAction.prototype.show);
        assert.notCalled(PageAction.prototype.hide);
      });
      it("should do nothing if the browser is not the `selectedBrowser`", () => {
        const someOtherFakeBrowser = {};
        CFRPageActions.updatePageActions(someOtherFakeBrowser);
        assert.notCalled(PageAction.prototype.show);
        assert.notCalled(PageAction.prototype.hide);
      });
      it("should hide the pageAction if a recommendation doesn't exist for the given browser", () => {
        CFRPageActions.RecommendationMap.delete(fakeBrowser);
        CFRPageActions.updatePageActions(fakeBrowser);
        assert.calledOnce(PageAction.prototype.hide);
      });
      it("should show the pageAction if a recommendation exists and the host matches", () => {
        CFRPageActions.updatePageActions(fakeBrowser);
        assert.calledOnce(PageAction.prototype.show);
        assert.calledWith(PageAction.prototype.show, savedRec);
      });
      it("should hide the pageAction and delete the recommendation if the recommendation exists but the host doesn't match", () => {
        const someOtherFakeHost = "subdomain.mozilla.com";
        fakeBrowser.documentURI.host = someOtherFakeHost;
        assert.isTrue(CFRPageActions.RecommendationMap.has(fakeBrowser));
        CFRPageActions.updatePageActions(fakeBrowser);
        assert.calledOnce(PageAction.prototype.hide);
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
    });

    describe("forceRecommendation", () => {
      it("should succeed and add an element to the RecommendationMap", async () => {
        assert.isTrue(await CFRPageActions.forceRecommendation({browser: fakeBrowser}, fakeRecommendation, dispatchStub));
        assert.deepInclude(CFRPageActions.RecommendationMap.get(fakeBrowser), {
          id: fakeRecommendation.id,
          content: fakeRecommendation.content,
        });
      });
      it("should create a PageAction if one doesn't exist for the window, save it in the PageActionMap, and call `show`", async () => {
        const win = fakeBrowser.ownerGlobal;
        assert.isFalse(CFRPageActions.PageActionMap.has(win));
        await CFRPageActions.forceRecommendation({browser: fakeBrowser}, fakeRecommendation, dispatchStub);
        const pageAction = CFRPageActions.PageActionMap.get(win);
        assert.equal(win, pageAction.window);
        assert.equal(dispatchStub, pageAction._dispatchToASRouter);
        assert.calledOnce(PageAction.prototype.show);
      });
    });

    describe("addRecommendation", () => {
      it("should fail and not add a recommendation if the browser is part of a private window", async () => {
        global.PrivateBrowsingUtils.isWindowPrivate.returns(true);
        assert.isFalse(await CFRPageActions.addRecommendation(fakeBrowser, fakeHost, fakeRecommendation, dispatchStub));
        assert.isFalse(CFRPageActions.RecommendationMap.has(fakeBrowser));
      });
      it("should fail and not add a recommendation if the browser is not the selected browser", async () => {
        global.gBrowser.selectedBrowser = {}; // Some other browser
        assert.isFalse(await CFRPageActions.addRecommendation(fakeBrowser, fakeHost, fakeRecommendation, dispatchStub));
      });
      it("should fail and not add a recommendation if the host doesn't match", async () => {
        const someOtherFakeHost = "subdomain.mozilla.com";
        assert.isFalse(await CFRPageActions.addRecommendation(fakeBrowser, someOtherFakeHost, fakeRecommendation, dispatchStub));
      });
      it("should otherwise succeed and add an element to the RecommendationMap", async () => {
        assert.isTrue(await CFRPageActions.addRecommendation(fakeBrowser, fakeHost, fakeRecommendation, dispatchStub));
        assert.deepInclude(CFRPageActions.RecommendationMap.get(fakeBrowser), {
          id: fakeRecommendation.id,
          host: fakeHost,
          content: fakeRecommendation.content,
        });
      });
      it("should create a PageAction if one doesn't exist for the window, save it in the PageActionMap, and call `show`", async () => {
        const win = fakeBrowser.ownerGlobal;
        assert.isFalse(CFRPageActions.PageActionMap.has(win));
        await CFRPageActions.addRecommendation(fakeBrowser, fakeHost, fakeRecommendation, dispatchStub);
        const pageAction = CFRPageActions.PageActionMap.get(win);
        assert.equal(win, pageAction.window);
        assert.equal(dispatchStub, pageAction._dispatchToASRouter);
        assert.calledOnce(PageAction.prototype.show);
      });
      it("should add the right url if we fetched and addon install URL", async () => {
        sinon.stub(CFRPageActions, "_fetchLatestAddonVersion").returns(Promise.resolve("latest-addon.xpi"));
        fakeRecommendation.template = "cfr_doorhanger";
        await CFRPageActions.addRecommendation(fakeBrowser, fakeHost, fakeRecommendation, dispatchStub);
        const recommendation = CFRPageActions.RecommendationMap.get(fakeBrowser);
        assert.equal(recommendation.content.buttons.primary.action.data.url, "latest-addon.xpi");

        // sanity check - just go through some of the rest of the attributes to make sure they were untouched
        assert.equal(recommendation.id, fakeRecommendation.id);
        assert.equal(recommendation.content.heading_text, fakeRecommendation.content.heading_text);
        assert.equal(recommendation.content.addon, fakeRecommendation.content.addon);
        assert.equal(recommendation.content.text, fakeRecommendation.content.text);
        assert.equal(recommendation.content.buttons.secondary, fakeRecommendation.content.buttons.secondary);
        assert.equal(recommendation.content.buttons.primary.action.id, fakeRecommendation.content.buttons.primary.action.id);

        delete fakeRecommendation.template;
      });
    });

    describe("clearRecommendations", () => {
      const createFakePageAction = () => ({hide: sandbox.stub()});
      const windows = [{}, {}, {closed: true}];
      const browsers = [{}, {}, {}, {}];

      beforeEach(() => {
        CFRPageActions.PageActionMap.set(windows[0], createFakePageAction());
        CFRPageActions.PageActionMap.set(windows[2], createFakePageAction());
        for (const browser of browsers) {
          CFRPageActions.RecommendationMap.set(browser, {});
        }
        globals.set({Services: {wm: {getEnumerator: () => windows}}});
      });

      it("should hide the PageActions of any existing, non-closed windows", () => {
        const pageActions = windows.map(win => CFRPageActions.PageActionMap.get(win));
        CFRPageActions.clearRecommendations();

        // Only the first window had a PageAction and wasn't closed
        assert.calledOnce(pageActions[0].hide);
        assert.isUndefined(pageActions[1]);
        assert.notCalled(pageActions[2].hide);
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
  });
});

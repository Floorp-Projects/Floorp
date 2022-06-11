import { combineReducers, createStore } from "redux";
import { INITIAL_STATE, reducers } from "common/Reducers.jsm";
import { Provider } from "react-redux";
import {
  _TopicsWidget as TopicsWidgetBase,
  TopicsWidget,
} from "content-src/components/DiscoveryStreamComponents/TopicsWidget/TopicsWidget";
import { SafeAnchor } from "content-src/components/DiscoveryStreamComponents/SafeAnchor/SafeAnchor";
import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { mount } from "enzyme";
import React from "react";

describe("Discovery Stream <TopicsWidget>", () => {
  let sandbox;
  let wrapper;
  let dispatch;
  let fakeWindow;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();
    fakeWindow = {
      innerWidth: 1000,
      innerHeight: 900,
    };

    wrapper = mount(
      <TopicsWidgetBase
        dispatch={dispatch}
        source="CARDGRID_WIDGET"
        position={2}
        id={1}
        windowObj={fakeWindow}
        DiscoveryStream={{
          experimentData: {
            utmCampaign: "utmCampaign",
            utmContent: "utmContent",
            utmSource: "utmSource",
          },
        }}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".ds-topics-widget"));
  });

  it("should connect with DiscoveryStream store", () => {
    let store = createStore(combineReducers(reducers), INITIAL_STATE);
    wrapper = mount(
      <Provider store={store}>
        <TopicsWidget />
      </Provider>
    );

    const topicsWidget = wrapper.find(TopicsWidgetBase);
    assert.ok(topicsWidget.exists());
    assert.lengthOf(topicsWidget, 1);
    assert.deepEqual(
      topicsWidget.props().DiscoveryStream.experimentData,
      INITIAL_STATE.DiscoveryStream.experimentData
    );
  });

  describe("dispatch", () => {
    it("should dispatch loaded event", () => {
      assert.callCount(dispatch, 1);
      const [first] = dispatch.getCalls();
      assert.calledWith(
        first,
        ac.DiscoveryStreamLoadedContent({
          source: "CARDGRID_WIDGET",
          tiles: [
            {
              id: 1,
              pos: 2,
            },
          ],
        })
      );
    });

    it("should dispatch click event for technology", () => {
      // Click technology topic.
      wrapper
        .find(SafeAnchor)
        .at(0)
        .simulate("click");

      // First call is DiscoveryStreamLoadedContent, which is already tested.
      const [second, third, fourth] = dispatch.getCalls().slice(1, 4);

      assert.callCount(dispatch, 4);
      assert.calledWith(
        second,
        ac.OnlyToMain({
          type: at.OPEN_LINK,
          data: {
            event: {
              altKey: undefined,
              button: undefined,
              ctrlKey: undefined,
              metaKey: undefined,
              shiftKey: undefined,
            },
            referrer: "https://getpocket.com/recommendations",
            url:
              "https://getpocket.com/explore/technology?utm_source=utmSource&utm_content=utmContent&utm_campaign=utmCampaign",
          },
        })
      );
      assert.calledWith(
        third,
        ac.UserEvent({
          event: "CLICK",
          source: "CARDGRID_WIDGET",
          action_position: 2,
          value: {
            card_type: "topics_widget",
            topic: "technology",
            position_in_card: 0,
          },
        })
      );
      assert.calledWith(
        fourth,
        ac.ImpressionStats({
          click: 0,
          source: "CARDGRID_WIDGET",
          tiles: [{ id: 1, pos: 2 }],
          window_inner_width: 1000,
          window_inner_height: 900,
        })
      );
    });

    it("should dispatch click event for must reads", () => {
      // Click must reads topic.
      wrapper
        .find(SafeAnchor)
        .at(8)
        .simulate("click");

      // First call is DiscoveryStreamLoadedContent, which is already tested.
      const [second, third, fourth] = dispatch.getCalls().slice(1, 4);

      assert.callCount(dispatch, 4);
      assert.calledWith(
        second,
        ac.OnlyToMain({
          type: at.OPEN_LINK,
          data: {
            event: {
              altKey: undefined,
              button: undefined,
              ctrlKey: undefined,
              metaKey: undefined,
              shiftKey: undefined,
            },
            referrer: "https://getpocket.com/recommendations",
            url:
              "https://getpocket.com/collections?utm_source=utmSource&utm_content=utmContent&utm_campaign=utmCampaign",
          },
        })
      );
      assert.calledWith(
        third,
        ac.UserEvent({
          event: "CLICK",
          source: "CARDGRID_WIDGET",
          action_position: 2,
          value: {
            card_type: "topics_widget",
            topic: "must-reads",
            position_in_card: 8,
          },
        })
      );
      assert.calledWith(
        fourth,
        ac.ImpressionStats({
          click: 0,
          source: "CARDGRID_WIDGET",
          tiles: [{ id: 1, pos: 2 }],
          window_inner_width: 1000,
          window_inner_height: 900,
        })
      );
    });

    it("should dispatch click event for more topics", () => {
      // Click more-topics.
      wrapper
        .find(SafeAnchor)
        .at(9)
        .simulate("click");

      // First call is DiscoveryStreamLoadedContent, which is already tested.
      const [second, third, fourth] = dispatch.getCalls().slice(1, 4);

      assert.callCount(dispatch, 4);
      assert.calledWith(
        second,
        ac.OnlyToMain({
          type: at.OPEN_LINK,
          data: {
            event: {
              altKey: undefined,
              button: undefined,
              ctrlKey: undefined,
              metaKey: undefined,
              shiftKey: undefined,
            },
            referrer: "https://getpocket.com/recommendations",
            url:
              "https://getpocket.com/explore?utm_source=utmSource&utm_content=utmContent&utm_campaign=utmCampaign",
          },
        })
      );
      assert.calledWith(
        third,
        ac.UserEvent({
          event: "CLICK",
          source: "CARDGRID_WIDGET",
          action_position: 2,
          value: { card_type: "topics_widget", topic: "more-topics" },
        })
      );
      assert.calledWith(
        fourth,
        ac.ImpressionStats({
          click: 0,
          source: "CARDGRID_WIDGET",
          tiles: [{ id: 1, pos: 2 }],
          window_inner_width: 1000,
          window_inner_height: 900,
        })
      );
    });
  });
});

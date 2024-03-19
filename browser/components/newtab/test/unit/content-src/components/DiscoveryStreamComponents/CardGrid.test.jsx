import {
  _CardGrid as CardGrid,
  IntersectionObserver,
  RecentSavesContainer,
  OnboardingExperience,
  DSSubHeader,
} from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import { combineReducers, createStore } from "redux";
import { INITIAL_STATE, reducers } from "common/Reducers.sys.mjs";
import { Provider } from "react-redux";
import {
  DSCard,
  PlaceholderDSCard,
} from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import { TopicsWidget } from "content-src/components/DiscoveryStreamComponents/TopicsWidget/TopicsWidget";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import React from "react";
import { shallow, mount } from "enzyme";

// Wrap this around any component that uses useSelector,
// or any mount that uses a child that uses redux.
function WrapWithProvider({ children, state = INITIAL_STATE }) {
  let store = createStore(combineReducers(reducers), state);
  return <Provider store={store}>{children}</Provider>;
}

describe("<CardGrid>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallow(
      <CardGrid
        Prefs={INITIAL_STATE.Prefs}
        DiscoveryStream={INITIAL_STATE.DiscoveryStream}
      />
    );
  });

  it("should render an empty div", () => {
    assert.ok(wrapper.exists());
    assert.lengthOf(wrapper.children(), 0);
  });

  it("should render DSCards", () => {
    wrapper.setProps({ items: 2, data: { recommendations: [{}, {}] } });

    assert.lengthOf(wrapper.find(".ds-card-grid").children(), 2);
    assert.equal(wrapper.find(".ds-card-grid").children().at(0).type(), DSCard);
  });

  it("should add 4 card classname to card grid", () => {
    wrapper.setProps({
      fourCardLayout: true,
      data: { recommendations: [{}, {}] },
    });

    assert.ok(wrapper.find(".ds-card-grid-four-card-variant").exists());
  });

  it("should add no description classname to card grid", () => {
    wrapper.setProps({
      hideCardBackground: true,
      data: { recommendations: [{}, {}] },
    });

    assert.ok(wrapper.find(".ds-card-grid-hide-background").exists());
  });

  it("should render sub header in the middle of the card grid for both regular and compact", () => {
    const commonProps = {
      essentialReadsHeader: true,
      editorsPicksHeader: true,
      items: 12,
      data: {
        recommendations: [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}],
      },
      Prefs: INITIAL_STATE.Prefs,
      DiscoveryStream: INITIAL_STATE.DiscoveryStream,
    };
    wrapper = mount(
      <WrapWithProvider>
        <CardGrid {...commonProps} />
      </WrapWithProvider>
    );

    assert.ok(wrapper.find(DSSubHeader).exists());

    wrapper.setProps({
      compact: true,
    });
    wrapper = mount(
      <WrapWithProvider>
        <CardGrid {...commonProps} compact={true} />
      </WrapWithProvider>
    );

    assert.ok(wrapper.find(DSSubHeader).exists());
  });

  it("should add/hide description classname to card grid", () => {
    wrapper.setProps({
      data: { recommendations: [{}, {}] },
    });

    assert.ok(wrapper.find(".ds-card-grid-include-descriptions").exists());

    wrapper.setProps({
      hideDescriptions: true,
      data: { recommendations: [{}, {}] },
    });

    assert.ok(!wrapper.find(".ds-card-grid-include-descriptions").exists());
  });

  it("should create a widget card", () => {
    wrapper.setProps({
      widgets: {
        positions: [{ index: 1 }],
        data: [{ type: "TopicsWidget" }],
      },
      data: {
        recommendations: [{}, {}, {}],
      },
    });

    assert.ok(wrapper.find(TopicsWidget).exists());
  });
});

// Build IntersectionObserver class with the arg `entries` for the intersect callback.
function buildIntersectionObserver(entries) {
  return class {
    constructor(callback) {
      this.callback = callback;
    }

    observe() {
      this.callback(entries);
    }

    unobserve() {}

    disconnect() {}
  };
}

describe("<IntersectionObserver>", () => {
  let wrapper;
  let fakeWindow;
  let intersectEntries;

  beforeEach(() => {
    intersectEntries = [{ isIntersecting: true }];
    fakeWindow = {
      IntersectionObserver: buildIntersectionObserver(intersectEntries),
    };
    wrapper = mount(<IntersectionObserver windowObj={fakeWindow} />);
  });

  it("should render an empty div", () => {
    assert.ok(wrapper.exists());
    assert.equal(wrapper.children().at(0).type(), "div");
  });

  it("should fire onIntersecting", () => {
    const onIntersecting = sinon.stub();
    wrapper = mount(
      <IntersectionObserver
        windowObj={fakeWindow}
        onIntersecting={onIntersecting}
      />
    );
    assert.calledOnce(onIntersecting);
  });
});

describe("<RecentSavesContainer>", () => {
  let wrapper;
  let fakeWindow;
  let intersectEntries;
  let dispatch;

  beforeEach(() => {
    dispatch = sinon.stub();
    intersectEntries = [{ isIntersecting: true }];
    fakeWindow = {
      IntersectionObserver: buildIntersectionObserver(intersectEntries),
    };
    wrapper = mount(
      <WrapWithProvider
        state={{
          DiscoveryStream: {
            isUserLoggedIn: true,
            recentSavesData: [
              {
                resolved_id: "resolved_id",
                top_image_url: "top_image_url",
                title: "title",
                resolved_url: "https://resolved_url",
                domain: "domain",
                excerpt: "excerpt",
              },
            ],
            experimentData: {
              utmSource: "utmSource",
              utmContent: "utmContent",
              utmCampaign: "utmCampaign",
            },
          },
        }}
      >
        <RecentSavesContainer
          gridClassName="ds-card-grid"
          windowObj={fakeWindow}
          dispatch={dispatch}
        />
      </WrapWithProvider>
    ).find(RecentSavesContainer);
  });

  it("should render an IntersectionObserver when not visible", () => {
    intersectEntries = [{ isIntersecting: false }];
    fakeWindow = {
      IntersectionObserver: buildIntersectionObserver(intersectEntries),
    };
    wrapper = mount(
      <WrapWithProvider>
        <RecentSavesContainer windowObj={fakeWindow} dispatch={dispatch} />
      </WrapWithProvider>
    ).find(RecentSavesContainer);

    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(IntersectionObserver).exists());
  });

  it("should render nothing if visible until we log in", () => {
    assert.ok(!wrapper.find(IntersectionObserver).exists());
    assert.calledOnce(dispatch);
    assert.calledWith(
      dispatch,
      ac.AlsoToMain({
        type: at.DISCOVERY_STREAM_POCKET_STATE_INIT,
      })
    );
  });

  it("should render a grid if visible and logged in", () => {
    assert.lengthOf(wrapper.find(".ds-card-grid"), 1);
    assert.lengthOf(wrapper.find(DSSubHeader), 1);
    assert.lengthOf(wrapper.find(PlaceholderDSCard), 2);
    assert.lengthOf(wrapper.find(DSCard), 3);
  });

  it("should render a my list link with proper utm params", () => {
    assert.equal(
      wrapper.find(".section-sub-link").at(0).prop("url"),
      "https://getpocket.com/a?utm_source=utmSource&utm_content=utmContent&utm_campaign=utmCampaign"
    );
  });

  it("should fire a UserEvent for my list clicks", () => {
    wrapper.find(".section-sub-link").at(0).simulate("click");
    assert.calledWith(
      dispatch,
      ac.DiscoveryStreamUserEvent({
        event: "CLICK",
        source: `CARDGRID_RECENT_SAVES_VIEW_LIST`,
      })
    );
  });
});

describe("<OnboardingExperience>", () => {
  let wrapper;
  let fakeWindow;
  let intersectEntries;
  let dispatch;
  let resizeCallback;

  let fakeResizeObserver = class {
    constructor(callback) {
      resizeCallback = callback;
    }

    observe() {}

    unobserve() {}

    disconnect() {}
  };

  beforeEach(() => {
    dispatch = sinon.stub();
    intersectEntries = [{ isIntersecting: true, intersectionRatio: 1 }];
    fakeWindow = {
      ResizeObserver: fakeResizeObserver,
      IntersectionObserver: buildIntersectionObserver(intersectEntries),
      document: {
        visibilityState: "visible",
        addEventListener: () => {},
        removeEventListener: () => {},
      },
    };
    wrapper = mount(
      <WrapWithProvider state={{}}>
        <OnboardingExperience windowObj={fakeWindow} dispatch={dispatch} />
      </WrapWithProvider>
    ).find(OnboardingExperience);
  });

  it("should render a ds-onboarding", () => {
    assert.ok(wrapper.exists());
    assert.lengthOf(wrapper.find(".ds-onboarding"), 1);
  });

  it("should dismiss on dismiss click", () => {
    wrapper.find(".ds-dismiss-button").simulate("click");

    assert.calledWith(
      dispatch,
      ac.DiscoveryStreamUserEvent({
        event: "BLOCK",
        source: "POCKET_ONBOARDING",
      })
    );
    assert.calledWith(
      dispatch,
      ac.SetPref("discoverystream.onboardingExperience.dismissed", true)
    );
    assert.equal(wrapper.getDOMNode().style["max-height"], "0px");
    assert.equal(wrapper.getDOMNode().style.opacity, "0");
  });

  it("should update max-height on resize", () => {
    sinon
      .stub(wrapper.find(".ds-onboarding-ref").getDOMNode(), "offsetHeight")
      .get(() => 123);
    resizeCallback();
    assert.equal(wrapper.getDOMNode().style["max-height"], "123px");
  });

  it("should fire intersection events", () => {
    assert.calledWith(
      dispatch,
      ac.DiscoveryStreamUserEvent({
        event: "IMPRESSION",
        source: "POCKET_ONBOARDING",
      })
    );
  });
});

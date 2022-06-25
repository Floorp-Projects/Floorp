import {
  _CardGrid as CardGrid,
  IntersectionObserver,
  RecentSavesContainer,
  DSSubHeader,
  GridContainer,
} from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import { combineReducers, createStore } from "redux";
import { INITIAL_STATE, reducers } from "common/Reducers.jsm";
import { Provider } from "react-redux";
import {
  DSCard,
  PlaceholderDSCard,
  LastCardMessage,
} from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import { TopicsWidget } from "content-src/components/DiscoveryStreamComponents/TopicsWidget/TopicsWidget";
import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
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
      <CardGrid DiscoveryStream={INITIAL_STATE.DiscoveryStream} />
    );
  });

  it("should render an empty div", () => {
    assert.ok(wrapper.exists());
    assert.lengthOf(wrapper.children(), 0);
  });

  it("should render DSCards", () => {
    wrapper.setProps({ items: 2, data: { recommendations: [{}, {}] } });

    assert.lengthOf(wrapper.find(GridContainer).children(), 2);
    assert.equal(
      wrapper
        .find(GridContainer)
        .children()
        .at(0)
        .type(),
      DSCard
    );
  });

  it("should add hero classname to card grid", () => {
    wrapper.setProps({
      display_variant: "hero",
      data: { recommendations: [{}, {}] },
    });

    assert.ok(wrapper.find(".ds-card-grid-hero").exists());
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
    wrapper = mount(
      <WrapWithProvider>
        <CardGrid
          essentialReadsHeader={true}
          editorsPicksHeader={true}
          items={12}
          data={{
            recommendations: [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}],
          }}
          DiscoveryStream={INITIAL_STATE.DiscoveryStream}
        />
      </WrapWithProvider>
    );

    assert.ok(wrapper.find(DSSubHeader).exists());

    wrapper.setProps({
      compact: true,
    });
    wrapper = mount(
      <WrapWithProvider>
        <CardGrid
          essentialReadsHeader={true}
          editorsPicksHeader={true}
          compact={true}
          items={12}
          data={{
            recommendations: [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}],
          }}
          DiscoveryStream={INITIAL_STATE.DiscoveryStream}
        />
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

  it("should show last card and more loaded state", () => {
    const dispatch = sinon.stub();
    wrapper.setProps({
      dispatch,
      compact: true,
      loadMore: true,
      lastCardMessageEnabled: true,
      loadMoreThreshold: 2,
      data: {
        recommendations: [{}, {}, {}],
      },
    });

    const loadMoreButton = wrapper.find(".ds-card-grid-load-more-button");
    assert.ok(loadMoreButton.exists());

    loadMoreButton.simulate("click", { preventDefault: () => {} });
    assert.calledOnce(dispatch);
    assert.calledWith(
      dispatch,
      ac.UserEvent({
        event: "CLICK",
        source: "DS_LOAD_MORE_BUTTON",
      })
    );

    const lastCard = wrapper.find(LastCardMessage);
    assert.ok(lastCard.exists());
  });

  it("should only show load more with more than threshold number of stories", () => {
    wrapper.setProps({
      loadMore: true,
      loadMoreThreshold: 2,
      data: {
        recommendations: [{}, {}, {}],
      },
    });

    let loadMoreButton = wrapper.find(".ds-card-grid-load-more-button");
    assert.ok(loadMoreButton.exists());

    wrapper.setProps({
      loadMoreThreshold: 3,
    });

    loadMoreButton = wrapper.find(".ds-card-grid-load-more-button");
    assert.ok(!loadMoreButton.exists());
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
    assert.equal(
      wrapper
        .children()
        .at(0)
        .type(),
      "div"
    );
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
    intersectEntries = [{ isIntersecting: false }];
    fakeWindow = {
      IntersectionObserver: buildIntersectionObserver(intersectEntries),
    };
    wrapper = mount(
      <WrapWithProvider>
        <RecentSavesContainer windowObj={fakeWindow} dispatch={dispatch} />
      </WrapWithProvider>
    ).find(RecentSavesContainer);
  });

  it("should render an IntersectionObserver when not visible", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(IntersectionObserver).exists());
  });

  it("should render a nothing if visible until we log in", () => {
    intersectEntries = [{ isIntersecting: true }];
    fakeWindow = {
      IntersectionObserver: buildIntersectionObserver(intersectEntries),
    };
    wrapper = mount(
      <WrapWithProvider>
        <RecentSavesContainer windowObj={fakeWindow} dispatch={dispatch} />
      </WrapWithProvider>
    ).find(RecentSavesContainer);
    assert.ok(!wrapper.find(IntersectionObserver).exists());
    assert.calledOnce(dispatch);
    assert.calledWith(
      dispatch,
      ac.AlsoToMain({
        type: at.DISCOVERY_STREAM_POCKET_STATE_INIT,
      })
    );
  });

  it("should render a GridContainer if visible and logged in", () => {
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
                resolved_url: "resolved_url",
                domain: "domain",
                excerpt: "excerpt",
              },
            ],
          },
        }}
      >
        <RecentSavesContainer windowObj={fakeWindow} dispatch={dispatch} />
      </WrapWithProvider>
    ).find(RecentSavesContainer);
    assert.lengthOf(wrapper.find(GridContainer), 1);
    assert.lengthOf(wrapper.find(PlaceholderDSCard), 2);
    assert.lengthOf(wrapper.find(DSCard), 3);
  });
});

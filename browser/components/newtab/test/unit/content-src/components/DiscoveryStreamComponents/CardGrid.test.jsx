import {
  CardGrid,
  DSSubHeader,
  GridContainer,
} from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import { combineReducers, createStore } from "redux";
import { INITIAL_STATE, reducers } from "common/Reducers.jsm";
import { Provider } from "react-redux";
import {
  DSCard,
  LastCardMessage,
} from "content-src/components/DiscoveryStreamComponents/DSCard/DSCard";
import { TopicsWidget } from "content-src/components/DiscoveryStreamComponents/TopicsWidget/TopicsWidget";
import { actionCreators as ac } from "common/Actions.jsm";
import React from "react";
import { shallow, mount } from "enzyme";

describe("<CardGrid>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = shallow(<CardGrid />);
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
    let store = createStore(combineReducers(reducers), INITIAL_STATE);
    wrapper = mount(
      <Provider store={store}>
        <CardGrid
          essentialReadsHeader={true}
          editorsPicksHeader={true}
          items={12}
          data={{
            recommendations: [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}],
          }}
        />
      </Provider>
    );

    assert.ok(wrapper.find(DSSubHeader).exists());

    wrapper.setProps({
      compact: true,
    });
    wrapper = mount(
      <Provider store={store}>
        <CardGrid
          essentialReadsHeader={true}
          editorsPicksHeader={true}
          compact={true}
          items={12}
          data={{
            recommendations: [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}],
          }}
        />
      </Provider>
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

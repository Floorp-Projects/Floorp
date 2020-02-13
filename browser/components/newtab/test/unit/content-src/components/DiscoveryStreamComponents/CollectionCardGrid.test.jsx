import { CollectionCardGrid } from "content-src/components/DiscoveryStreamComponents/CollectionCardGrid/CollectionCardGrid";
import { CardGrid } from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import { combineReducers, createStore } from "redux";
import { INITIAL_STATE, reducers } from "common/Reducers.jsm";
import { Provider } from "react-redux";
import React from "react";
import { mount } from "enzyme";

function mountCollectionWithProps(props, spocsState) {
  const state = {
    ...INITIAL_STATE,
    DiscoveryStream: {
      ...INITIAL_STATE.DiscoveryStream,
      spocs: {
        ...INITIAL_STATE.DiscoveryStream.spocs,
        ...spocsState,
      },
    },
  };
  const store = createStore(combineReducers(reducers), state);

  return mount(
    <Provider store={store}>
      <CollectionCardGrid {...props} />
    </Provider>
  );
}

describe("<CollectionCardGrid>", () => {
  let wrapper;

  beforeEach(() => {
    const initialSpocs = [{ id: 123 }, { id: 456 }, { id: 789 }];
    wrapper = mountCollectionWithProps(
      {
        placement: {
          name: "spocs",
        },
        data: {
          spocs: initialSpocs,
        },
      },
      {
        data: {
          spocs: {
            title: "title",
            context: "context",
            items: initialSpocs,
          },
        },
      }
    );
  });

  it("should render an empty div", () => {
    wrapper = mountCollectionWithProps({}, {});
    assert.ok(wrapper.exists());
    assert.ok(!wrapper.exists(".ds-collection-card-grid"));
  });

  it("should render a CardGrid", () => {
    assert.lengthOf(wrapper.find(".ds-collection-card-grid").children(), 1);
    assert.equal(
      wrapper
        .find(".ds-collection-card-grid")
        .children()
        .at(0)
        .type(),
      CardGrid
    );
  });

  it("should inject spocs in every CardGrid rec position", () => {
    assert.lengthOf(
      wrapper
        .find(".ds-collection-card-grid")
        .children()
        .at(0)
        .props().data.recommendations,
      3
    );
  });

  it("should pass along title and context to CardGrid", () => {
    assert.equal(
      wrapper
        .find(".ds-collection-card-grid")
        .children()
        .at(0)
        .props().title,
      "title"
    );

    assert.equal(
      wrapper
        .find(".ds-collection-card-grid")
        .children()
        .at(0)
        .props().context,
      "context"
    );
  });
});

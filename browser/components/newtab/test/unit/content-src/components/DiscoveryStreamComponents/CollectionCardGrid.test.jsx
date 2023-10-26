import { CollectionCardGrid } from "content-src/components/DiscoveryStreamComponents/CollectionCardGrid/CollectionCardGrid";
import { CardGrid } from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import React from "react";
import { shallow } from "enzyme";

describe("<CollectionCardGrid>", () => {
  let wrapper;
  let sandbox;
  let dispatchStub;
  const initialSpocs = [
    { id: 123, url: "123" },
    { id: 456, url: "456" },
    { id: 789, url: "789" },
  ];

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatchStub = sandbox.stub();
    wrapper = shallow(
      <CollectionCardGrid
        dispatch={dispatchStub}
        type="COLLECTIONCARDGRID"
        placement={{
          name: "spocs",
        }}
        data={{
          spocs: initialSpocs,
        }}
        spocs={{
          data: {
            spocs: {
              title: "title",
              context: "context",
              items: initialSpocs,
            },
          },
        }}
      />
    );
  });

  it("should render an empty div", () => {
    wrapper = shallow(<CollectionCardGrid />);
    assert.ok(wrapper.exists());
    assert.ok(!wrapper.exists(".ds-collection-card-grid"));
  });

  it("should render a CardGrid", () => {
    assert.lengthOf(wrapper.find(".ds-collection-card-grid").children(), 1);
    assert.equal(
      wrapper.find(".ds-collection-card-grid").children().at(0).type(),
      CardGrid
    );
  });

  it("should inject spocs in every CardGrid rec position", () => {
    assert.lengthOf(
      wrapper.find(".ds-collection-card-grid").children().at(0).props().data
        .recommendations,
      3
    );
  });

  it("should pass along title and context to CardGrid", () => {
    assert.equal(
      wrapper.find(".ds-collection-card-grid").children().at(0).props().title,
      "title"
    );

    assert.equal(
      wrapper.find(".ds-collection-card-grid").children().at(0).props().context,
      "context"
    );
  });

  it("should render nothing without a title", () => {
    wrapper = shallow(
      <CollectionCardGrid
        dispatch={dispatchStub}
        placement={{
          name: "spocs",
        }}
        data={{
          spocs: initialSpocs,
        }}
        spocs={{
          data: {
            spocs: {
              title: "",
              context: "context",
              items: initialSpocs,
            },
          },
        }}
      />
    );

    assert.ok(wrapper.exists());
    assert.ok(!wrapper.exists(".ds-collection-card-grid"));
  });

  it("should dispatch telemety events on dismiss", () => {
    wrapper.instance().onDismissClick();

    const firstCall = dispatchStub.getCall(0);
    const secondCall = dispatchStub.getCall(1);
    const thirdCall = dispatchStub.getCall(2);

    assert.equal(firstCall.args[0].type, "BLOCK_URL");
    let expected = ["123", "456", "789"].map(url => ({
      url,
      pocket_id: undefined,
      isSponsoredTopSite: undefined,
      position: 0,
      is_pocket_card: false,
    }));

    assert.deepEqual(firstCall.args[0].data, expected);

    assert.equal(secondCall.args[0].type, "DISCOVERY_STREAM_USER_EVENT");
    assert.deepEqual(secondCall.args[0].data, {
      event: "BLOCK",
      source: "COLLECTIONCARDGRID",
      action_position: 0,
    });

    assert.equal(thirdCall.args[0].type, "TELEMETRY_IMPRESSION_STATS");
    assert.deepEqual(thirdCall.args[0].data, {
      source: "COLLECTIONCARDGRID",
      block: 0,
      tiles: [
        { id: 123, pos: 0 },
        { id: 456, pos: 1 },
        { id: 789, pos: 2 },
      ],
    });
  });
});

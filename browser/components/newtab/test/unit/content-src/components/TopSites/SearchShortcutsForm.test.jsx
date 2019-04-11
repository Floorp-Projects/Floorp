import {SearchShortcutsForm, SelectableSearchShortcut} from "content-src/components/TopSites/SearchShortcutsForm";
import React from "react";
import {shallow} from "enzyme";

describe("<SearchShortcutsForm>", () => {
  let wrapper;
  let sandbox;
  let dispatchStub;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatchStub = sandbox.stub();
    const defaultProps = {rows: [], searchShortcuts: []};
    wrapper = shallow(<SearchShortcutsForm TopSites={defaultProps} dispatch={dispatchStub} />);
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".topsite-form").exists());
  });

  it("should render SelectableSearchShortcut components", () => {
    wrapper.setState({shortcuts: [{}, {}]});

    assert.lengthOf(wrapper.find(".search-shortcuts-container div").children(), 2);
    assert.equal(wrapper.find(".search-shortcuts-container div").children().at(0)
      .type(), SelectableSearchShortcut);
  });

  it("should render SelectableSearchShortcut components", () => {
    const onCloseStub = sandbox.stub();
    const fakeEvent = {preventDefault: sandbox.stub()};
    wrapper.setState({shortcuts: [{}, {}]});
    wrapper.setProps({onClose: onCloseStub});

    wrapper.find(".done").simulate("click", fakeEvent);

    assert.calledOnce(dispatchStub);
    assert.calledOnce(fakeEvent.preventDefault);
    assert.calledOnce(onCloseStub);
  });
});

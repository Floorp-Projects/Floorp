import { A11yLinkButton } from "content-src/components/A11yLinkButton/A11yLinkButton";
import {
  ErrorBoundary,
  ErrorBoundaryFallback,
} from "content-src/components/ErrorBoundary/ErrorBoundary";
import React from "react";
import { shallow } from "enzyme";

describe("<ErrorBoundary>", () => {
  it("should render its children if componentDidCatch wasn't called", () => {
    const wrapper = shallow(
      <ErrorBoundary>
        <div className="kids" />
      </ErrorBoundary>
    );

    assert.lengthOf(wrapper.find(".kids"), 1);
  });

  it("should render ErrorBoundaryFallback if componentDidCatch called", () => {
    const wrapper = shallow(<ErrorBoundary />);

    wrapper.instance().componentDidCatch();
    // since shallow wrappers don't automatically manage lifecycle semantics:
    wrapper.update();

    assert.lengthOf(wrapper.find(ErrorBoundaryFallback), 1);
  });

  it("should render the given FallbackComponent if componentDidCatch called", () => {
    class TestFallback extends React.PureComponent {
      render() {
        return <div className="my-fallback">doh!</div>;
      }
    }

    const wrapper = shallow(<ErrorBoundary FallbackComponent={TestFallback} />);
    wrapper.instance().componentDidCatch();
    // since shallow wrappers don't automatically manage lifecycle semantics:
    wrapper.update();

    assert.lengthOf(wrapper.find(TestFallback), 1);
  });

  it("should pass the given className prop to the FallbackComponent", () => {
    class TestFallback extends React.PureComponent {
      render() {
        return <div className={this.props.className}>doh!</div>;
      }
    }

    const wrapper = shallow(
      <ErrorBoundary FallbackComponent={TestFallback} className="sheep" />
    );
    wrapper.instance().componentDidCatch();
    // since shallow wrappers don't automatically manage lifecycle semantics:
    wrapper.update();

    assert.lengthOf(wrapper.find(".sheep"), 1);
  });
});

describe("ErrorBoundaryFallback", () => {
  it("should render a <div> with a class of as-error-fallback", () => {
    const wrapper = shallow(<ErrorBoundaryFallback />);

    assert.lengthOf(wrapper.find("div.as-error-fallback"), 1);
  });

  it("should render a <div> with the props.className and .as-error-fallback", () => {
    const wrapper = shallow(<ErrorBoundaryFallback className="monkeys" />);

    assert.lengthOf(wrapper.find("div.monkeys.as-error-fallback"), 1);
  });

  it("should call window.location.reload(true) if .reload-button clicked", () => {
    const fakeWindow = { location: { reload: sinon.spy() } };
    const wrapper = shallow(<ErrorBoundaryFallback windowObj={fakeWindow} />);

    wrapper.find(".reload-button").simulate("click");

    assert.calledOnce(fakeWindow.location.reload);
    assert.calledWithExactly(fakeWindow.location.reload, true);
  });

  it("should render .reload-button as an <A11yLinkButton>", () => {
    const wrapper = shallow(<ErrorBoundaryFallback />);

    assert.lengthOf(wrapper.find("A11yLinkButton.reload-button"), 1);
  });

  it("should render newtab-error-fallback-refresh-link node", () => {
    const wrapper = shallow(<ErrorBoundaryFallback />);

    const msgWrapper = wrapper.find(
      '[data-l10n-id="newtab-error-fallback-refresh-link"]'
    );
    assert.lengthOf(msgWrapper, 1);
    assert.isTrue(msgWrapper.is(A11yLinkButton));
  });

  it("should render newtab-error-fallback-info node", () => {
    const wrapper = shallow(<ErrorBoundaryFallback />);

    const msgWrapper = wrapper.find(
      '[data-l10n-id="newtab-error-fallback-info"]'
    );
    assert.lengthOf(msgWrapper, 1);
  });
});

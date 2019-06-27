import { FluentOrText } from "content-src/components/FluentOrText/FluentOrText";
import React from "react";
import { shallow } from "enzyme";

describe("<FluentOrText>", () => {
  it("should create span with no children", () => {
    const wrapper = shallow(<FluentOrText />);

    assert.ok(wrapper.find("span"));
  });
  it("should set plain text", () => {
    const wrapper = shallow(<FluentOrText message={"hello"} />);

    assert.equal(wrapper.text(), "hello");
  });
  it("should use fluent id on automatic span", () => {
    const wrapper = shallow(<FluentOrText message={{ id: "fluent" }} />);

    assert.ok(wrapper.find("span[data-l10n-id='fluent']"));
  });
  it("should also allow string_id", () => {
    const wrapper = shallow(<FluentOrText message={{ string_id: "fluent" }} />);

    assert.ok(wrapper.find("span[data-l10n-id='fluent']"));
  });
  it("should use fluent id on child", () => {
    const wrapper = shallow(
      <FluentOrText message={{ id: "fluent" }}>
        <p />
      </FluentOrText>
    );

    assert.ok(wrapper.find("p[data-l10n-id='fluent']"));
  });
  it("should set args for fluent", () => {
    const wrapper = shallow(<FluentOrText message={{ args: { num: 5 } }} />);

    assert.ok(wrapper.find("span[data-l10n-args='{num: 5}']"));
  });
  it("should also allow values", () => {
    const wrapper = shallow(<FluentOrText message={{ values: { num: 5 } }} />);

    assert.ok(wrapper.find("span[data-l10n-args='{num: 5}']"));
  });
  it("should preserve original children with fluent", () => {
    const wrapper = shallow(
      <FluentOrText message={{ id: "fluent" }}>
        <p>
          <b data-l10n-name="bold" />
        </p>
      </FluentOrText>
    );

    assert.ok(wrapper.find("b[data-l10n-name='bold']"));
  });
  it("should only allow a single child", () => {
    assert.throws(() =>
      shallow(
        <FluentOrText>
          <p />
          <p />
        </FluentOrText>
      )
    );
  });
});

import {
  DSContextFooter,
  StatusMessage,
} from "content-src/components/DiscoveryStreamComponents/DSContextFooter/DSContextFooter";
import React from "react";
import { mount } from "enzyme";
import { cardContextTypes } from "content-src/components/Card/types.js";

describe("<DSContextFooter>", () => {
  let wrapper;
  let sandbox;
  const bookmarkBadge = "bookmark";
  const removeBookmarkBadge = "removedBookmark";
  const context = "Sponsored by Babel";

  beforeEach(() => {
    wrapper = mount(<DSContextFooter />);
    sandbox = sinon.createSandbox();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.isTrue(wrapper.exists());
    assert.isOk(wrapper.find(".story-footer"));
  });
  it("should render a badge if a proper badge prop is passed", () => {
    wrapper = mount(<DSContextFooter context_type={bookmarkBadge} />);
    const { fluentID } = cardContextTypes[bookmarkBadge];

    const statusLabel = wrapper.find(".story-context-label");
    assert.isOk(statusLabel);
    assert.equal(statusLabel.prop("data-l10n-id"), fluentID);
  });
  it("should only render a sponsored context if pass a sponsored context", async () => {
    wrapper = mount(
      <DSContextFooter context_type={bookmarkBadge} context={context} />
    );

    assert.lengthOf(wrapper.find(StatusMessage), 0);
    assert.equal(wrapper.find(".story-sponsored-label").text(), context);
  });
  it("should render a new badge if props change from an old badge to a new one", async () => {
    wrapper = mount(<DSContextFooter context_type={bookmarkBadge} />);

    const { fluentID: bookmarkFluentID } = cardContextTypes[bookmarkBadge];
    const bookmarkStatusMessage = wrapper.find(
      `div[data-l10n-id='${bookmarkFluentID}']`
    );
    assert.isTrue(bookmarkStatusMessage.exists());

    const { fluentID: removeBookmarkFluentID } = cardContextTypes[
      removeBookmarkBadge
    ];

    wrapper.setProps({ context_type: removeBookmarkBadge });
    await wrapper.update();

    assert.isEmpty(bookmarkStatusMessage);
    const removedBookmarkStatusMessage = wrapper.find(
      `div[data-l10n-id='${removeBookmarkFluentID}']`
    );
    assert.isTrue(removedBookmarkStatusMessage.exists());
  });
});

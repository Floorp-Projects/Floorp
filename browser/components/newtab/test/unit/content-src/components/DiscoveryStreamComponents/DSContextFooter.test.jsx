import {
  DSContextFooter,
  StatusMessage,
  DSMessageFooter,
} from "content-src/components/DiscoveryStreamComponents/DSContextFooter/DSContextFooter";
import React from "react";
import { mount } from "enzyme";
import { cardContextTypes } from "content-src/components/Card/types.mjs";
import { FluentOrText } from "content-src/components/FluentOrText/FluentOrText.jsx";

describe("<DSContextFooter>", () => {
  let wrapper;
  let sandbox;
  const bookmarkBadge = "bookmark";
  const removeBookmarkBadge = "removedBookmark";
  const context = "Sponsored by Babel";
  const sponsored_by_override = "Sponsored override";
  const engagement = "Popular";

  beforeEach(() => {
    wrapper = mount(<DSContextFooter />);
    sandbox = sinon.createSandbox();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => assert.isTrue(wrapper.exists()));
  it("should not render an engagement status if display_engagement_labels is false", () => {
    wrapper = mount(
      <DSContextFooter
        display_engagement_labels={false}
        engagement={engagement}
      />
    );

    const engagementLabel = wrapper.find(".story-view-count");
    assert.equal(engagementLabel.length, 0);
  });
  it("should render a badge if a proper badge prop is passed", () => {
    wrapper = mount(
      <DSContextFooter context_type={bookmarkBadge} engagement={engagement} />
    );
    const { fluentID } = cardContextTypes[bookmarkBadge];

    assert.lengthOf(wrapper.find(".story-view-count"), 0);
    const statusLabel = wrapper.find(".story-context-label");
    assert.equal(statusLabel.prop("data-l10n-id"), fluentID);
  });
  it("should only render a sponsored context if pass a sponsored context", async () => {
    wrapper = mount(
      <DSContextFooter
        context_type={bookmarkBadge}
        context={context}
        engagement={engagement}
      />
    );

    assert.lengthOf(wrapper.find(".story-view-count"), 0);
    assert.lengthOf(wrapper.find(StatusMessage), 0);
    assert.equal(wrapper.find(".story-sponsored-label").text(), context);
  });
  it("should render a sponsored_by_override if passed a sponsored_by_override", async () => {
    wrapper = mount(
      <DSContextFooter
        context_type={bookmarkBadge}
        context={context}
        sponsored_by_override={sponsored_by_override}
        engagement={engagement}
      />
    );

    assert.equal(
      wrapper.find(".story-sponsored-label").text(),
      sponsored_by_override
    );
  });
  it("should render nothing with a sponsored_by_override empty string", async () => {
    wrapper = mount(
      <DSContextFooter
        context_type={bookmarkBadge}
        context={context}
        sponsored_by_override=""
        engagement={engagement}
      />
    );

    assert.isFalse(wrapper.find(".story-sponsored-label").exists());
  });
  it("should render localized string with sponsor with no sponsored_by_override", async () => {
    wrapper = mount(
      <DSContextFooter
        context_type={bookmarkBadge}
        context={context}
        sponsor="Nimoy"
        engagement={engagement}
      />
    );

    assert.equal(
      wrapper.find(".story-sponsored-label").children().at(0).type(),
      FluentOrText
    );
  });
  it("should render a new badge if props change from an old badge to a new one", async () => {
    wrapper = mount(<DSContextFooter context_type={bookmarkBadge} />);

    const { fluentID: bookmarkFluentID } = cardContextTypes[bookmarkBadge];
    const bookmarkStatusMessage = wrapper.find(
      `div[data-l10n-id='${bookmarkFluentID}']`
    );
    assert.isTrue(bookmarkStatusMessage.exists());

    const { fluentID: removeBookmarkFluentID } =
      cardContextTypes[removeBookmarkBadge];

    wrapper.setProps({ context_type: removeBookmarkBadge });
    await wrapper.update();

    assert.isEmpty(bookmarkStatusMessage);
    const removedBookmarkStatusMessage = wrapper.find(
      `div[data-l10n-id='${removeBookmarkFluentID}']`
    );
    assert.isTrue(removedBookmarkStatusMessage.exists());
  });
  it("should render a story footer", () => {
    wrapper = mount(
      <DSMessageFooter
        context_type={bookmarkBadge}
        engagement={engagement}
        display_engagement_labels={true}
      />
    );

    assert.lengthOf(wrapper.find(".story-footer"), 1);
  });
});

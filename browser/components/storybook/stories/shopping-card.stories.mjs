/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html, ifDefined } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "browser/components/shopping/content/shopping-card.mjs";

export default {
  title: "Domain-specific UI Widgets/Shopping/Shopping Card",
  component: "shopping-card",
  parameters: {
    status: "in-development",
    fluent: `
shopping-card-label =
  .label = This the label
shopping-show-more-button = Show more
shopping-show-less-button = Show less
    `,
  },
};

const Template = ({ l10nId, rating, content, type }) => html`
  <main style="max-width: 400px">
    <shopping-card
      type=${ifDefined(type)}
      data-l10n-id=${ifDefined(l10nId)}
      data-l10n-attrs="label"
    >
      <div slot="rating">
        ${rating ? html`<moz-five-star rating="${rating}" />` : ""}
      </div>
      <div slot="content">${ifDefined(content)}</div>
    </shopping-card>
  </main>
`;

export const DefaultCard = Template.bind({});
DefaultCard.args = {
  l10nId: "shopping-card-label",
  content: "This is the content",
};

export const CardWithRating = Template.bind({});
CardWithRating.args = {
  ...DefaultCard.args,
  rating: 3,
};

export const CardOnlyContent = Template.bind({});
CardOnlyContent.args = {
  content: "This card only contains content",
};

export const CardTypeAccordion = Template.bind({});
CardTypeAccordion.args = {
  ...DefaultCard.args,
  content: `Lorem ipsum dolor sit amet, consectetur adipiscing elit.
  Nunc velit turpis, mollis a ultricies vitae, accumsan ut augue.
  In a eros ac dolor hendrerit varius et at mauris.`,
  type: "accordion",
};

export const CardTypeShowMoreButtonDisabled = Template.bind({});
CardTypeShowMoreButtonDisabled.args = {
  ...DefaultCard.args,
  content: `Lorem ipsum dolor sit amet, consectetur adipiscing elit.
  Nunc velit turpis, mollis a ultricies vitae, accumsan ut augue.
  In a eros ac dolor hendrerit varius et at mauris.`,
  type: "show-more",
};

export const CardTypeShowMore = Template.bind({});
CardTypeShowMore.args = {
  ...DefaultCard.args,
  content: `Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
  Posuere morbi leo urna molestie at elementum.
  Felis donec et odio pellentesque.
  Malesuada fames ac turpis egestas maecenas pharetra convallis posuere morbi. Varius duis at consectetur lorem donec massa sapien faucibus et.
  Et tortor consequat id porta nibh venenatis.
  Adipiscing diam donec adipiscing tristique risus nec feugiat in fermentum.
  Viverra accumsan in nisl nisi scelerisque eu ultrices vitae.
  Gravida neque convallis a cras.
  Fringilla est ullamcorper eget nulla.
  Habitant morbi tristique senectus et netus.
  Quam vulputate dignissim suspendisse in est ante in.
  Feugiat vivamus at augue eget arcu dictum varius duis.
  Est pellentesque elit ullamcorper dignissim cras tincidunt lobortis feugiat vivamus. Ultricies integer quis auctor elit.`,
  type: "show-more",
};

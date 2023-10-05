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

export const CardTypeShowMore = Template.bind({});
CardTypeShowMore.args = {
  ...DefaultCard.args,
  content: `Lorem ipsum dolor sit amet, consectetur adipiscing elit.
  Nunc velit turpis, mollis a ultricies vitae, accumsan ut augue.
  In a eros ac dolor hendrerit varius et at mauris.`,
  type: "show-more",
};

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac } from "common/Actions.jsm";
import { CardGrid } from "content-src/components/DiscoveryStreamComponents/CardGrid/CardGrid";
import { CollectionCardGrid } from "content-src/components/DiscoveryStreamComponents/CollectionCardGrid/CollectionCardGrid";
import { CollapsibleSection } from "content-src/components/CollapsibleSection/CollapsibleSection";
import { connect } from "react-redux";
import { DSMessage } from "content-src/components/DiscoveryStreamComponents/DSMessage/DSMessage";
import { DSPrivacyModal } from "content-src/components/DiscoveryStreamComponents/DSPrivacyModal/DSPrivacyModal";
import { DSTextPromo } from "content-src/components/DiscoveryStreamComponents/DSTextPromo/DSTextPromo";
import { Hero } from "content-src/components/DiscoveryStreamComponents/Hero/Hero";
import { Highlights } from "content-src/components/DiscoveryStreamComponents/Highlights/Highlights";
import { HorizontalRule } from "content-src/components/DiscoveryStreamComponents/HorizontalRule/HorizontalRule";
import { List } from "content-src/components/DiscoveryStreamComponents/List/List";
import { Navigation } from "content-src/components/DiscoveryStreamComponents/Navigation/Navigation";
import React from "react";
import { SectionTitle } from "content-src/components/DiscoveryStreamComponents/SectionTitle/SectionTitle";
import { selectLayoutRender } from "content-src/lib/selectLayoutRender";
import { TopSites } from "content-src/components/DiscoveryStreamComponents/TopSites/TopSites";

const ALLOWED_CSS_URL_PREFIXES = [
  "chrome://",
  "resource://",
  "https://img-getpocket.cdn.mozilla.net/",
];
const DUMMY_CSS_SELECTOR = "DUMMY#CSS.SELECTOR";
let rollCache = []; // Cache of random probability values for a spoc position

/**
 * Validate a CSS declaration. The values are assumed to be normalized by CSSOM.
 */
export function isAllowedCSS(property, value) {
  // Bug 1454823: INTERNAL properties, e.g., -moz-context-properties, are
  // exposed but their values aren't resulting in getting nothing. Fortunately,
  // we don't care about validating the values of the current set of properties.
  if (value === undefined) {
    return true;
  }

  // Make sure all urls are of the allowed protocols/prefixes
  const urls = value.match(/url\("[^"]+"\)/g);
  return (
    !urls ||
    urls.every(url =>
      ALLOWED_CSS_URL_PREFIXES.some(prefix => url.slice(5).startsWith(prefix))
    )
  );
}

export class _DiscoveryStreamBase extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onStyleMount = this.onStyleMount.bind(this);
  }

  onStyleMount(style) {
    // Unmounting style gets rid of old styles, so nothing else to do
    if (!style) {
      return;
    }

    const { sheet } = style;
    const styles = JSON.parse(style.dataset.styles);
    styles.forEach((row, rowIndex) => {
      row.forEach((component, componentIndex) => {
        // Nothing to do without optional styles overrides
        if (!component) {
          return;
        }

        Object.entries(component).forEach(([selectors, declarations]) => {
          // Start with a dummy rule to validate declarations and selectors
          sheet.insertRule(`${DUMMY_CSS_SELECTOR} {}`);
          const [rule] = sheet.cssRules;

          // Validate declarations and remove any offenders. CSSOM silently
          // discards invalid entries, so here we apply extra restrictions.
          rule.style = declarations;
          [...rule.style].forEach(property => {
            const value = rule.style[property];
            if (!isAllowedCSS(property, value)) {
              console.error(`Bad CSS declaration ${property}: ${value}`); // eslint-disable-line no-console
              rule.style.removeProperty(property);
            }
          });

          // Set the actual desired selectors scoped to the component
          const prefix = `.ds-layout > .ds-column:nth-child(${rowIndex +
            1}) .ds-column-grid > :nth-child(${componentIndex + 1})`;
          // NB: Splitting on "," doesn't work with strings with commas, but
          // we're okay with not supporting those selectors
          rule.selectorText = selectors
            .split(",")
            .map(
              selector =>
                prefix +
                // Assume :pseudo-classes are for component instead of descendant
                (selector[0] === ":" ? "" : " ") +
                selector
            )
            .join(",");

          // CSSOM silently ignores bad selectors, so we'll be noisy instead
          if (rule.selectorText === DUMMY_CSS_SELECTOR) {
            console.error(`Bad CSS selector ${selectors}`); // eslint-disable-line no-console
          }
        });
      });
    });
  }

  renderComponent(component, embedWidth) {
    const ENGAGEMENT_LABEL_ENABLED = this.props.Prefs.values[
      `discoverystream.engagementLabelEnabled`
    ];

    switch (component.type) {
      case "Highlights":
        return <Highlights />;
      case "TopSites":
        let promoAlignment;
        if (
          component.spocs &&
          component.spocs.positions &&
          component.spocs.positions.length
        ) {
          promoAlignment =
            component.spocs.positions[0].index === 0 ? "left" : "right";
        }
        return (
          <TopSites
            header={component.header}
            data={component.data}
            promoAlignment={promoAlignment}
          />
        );
      case "TextPromo":
        return (
          <DSTextPromo
            dispatch={this.props.dispatch}
            type={component.type}
            data={component.data}
          />
        );
      case "Message":
        return (
          <DSMessage
            title={component.header && component.header.title}
            subtitle={component.header && component.header.subtitle}
            link_text={component.header && component.header.link_text}
            link_url={component.header && component.header.link_url}
            icon={component.header && component.header.icon}
          />
        );
      case "SectionTitle":
        return <SectionTitle header={component.header} />;
      case "Navigation":
        return (
          <Navigation
            links={component.properties.links}
            alignment={component.properties.alignment}
            header={component.header}
          />
        );
      case "CollectionCardGrid":
        const { DiscoveryStream } = this.props;
        return (
          <CollectionCardGrid
            data={component.data}
            feed={component.feed}
            spocs={DiscoveryStream.spocs}
            placement={component.placement}
            border={component.properties.border}
            type={component.type}
            items={component.properties.items}
            cta_variant={component.cta_variant}
            display_engagement_labels={ENGAGEMENT_LABEL_ENABLED}
            dismissible={this.props.DiscoveryStream.isCollectionDismissible}
            dispatch={this.props.dispatch}
          />
        );
      case "CardGrid":
        return (
          <CardGrid
            title={component.header && component.header.title}
            display_variant={component.properties.display_variant}
            data={component.data}
            feed={component.feed}
            border={component.properties.border}
            type={component.type}
            dispatch={this.props.dispatch}
            items={component.properties.items}
            cta_variant={component.cta_variant}
            display_engagement_labels={ENGAGEMENT_LABEL_ENABLED}
          />
        );
      case "Hero":
        return (
          <Hero
            subComponentType={embedWidth >= 9 ? `cards` : `list`}
            feed={component.feed}
            title={component.header && component.header.title}
            data={component.data}
            border={component.properties.border}
            type={component.type}
            dispatch={this.props.dispatch}
            items={component.properties.items}
          />
        );
      case "HorizontalRule":
        return <HorizontalRule />;
      case "List":
        return (
          <List
            data={component.data}
            feed={component.feed}
            fullWidth={component.properties.full_width}
            hasBorders={component.properties.border === "border"}
            hasImages={component.properties.has_images}
            hasNumbers={component.properties.has_numbers}
            items={component.properties.items}
            type={component.type}
            header={component.header}
          />
        );
      default:
        return <div>{component.type}</div>;
    }
  }

  renderStyles(styles) {
    // Use json string as both the key and styles to render so React knows when
    // to unmount and mount a new instance for new styles.
    const json = JSON.stringify(styles);
    return <style key={json} data-styles={json} ref={this.onStyleMount} />;
  }

  componentWillReceiveProps(oldProps) {
    if (this.props.DiscoveryStream.layout !== oldProps.DiscoveryStream.layout) {
      rollCache = [];
    }
  }

  render() {
    // Select layout render data by adding spocs and position to recommendations
    const { layoutRender, spocsFill } = selectLayoutRender({
      state: this.props.DiscoveryStream,
      prefs: this.props.Prefs.values,
      rollCache,
      locale: this.props.locale,
    });
    const { config, spocs, feeds } = this.props.DiscoveryStream;

    // Send SPOCS Fill if any. Note that it should not send it again if the same
    // page gets re-rendered by state changes.
    if (
      spocs.loaded &&
      feeds.loaded &&
      spocsFill.length &&
      !this._spocsFillSent
    ) {
      this.props.dispatch(
        ac.DiscoveryStreamSpocsFill({ spoc_fills: spocsFill })
      );
      this._spocsFillSent = true;
    }

    // Allow rendering without extracting special components
    if (!config.collapsible) {
      return this.renderLayout(layoutRender);
    }

    // Find the first component of a type and remove it from layout
    const extractComponent = type => {
      for (const [rowIndex, row] of Object.entries(layoutRender)) {
        for (const [index, component] of Object.entries(row.components)) {
          if (component.type === type) {
            // Remove the row if it was the only component or the single item
            if (row.components.length === 1) {
              layoutRender.splice(rowIndex, 1);
            } else {
              row.components.splice(index, 1);
            }
            return component;
          }
        }
      }
      return null;
    };

    // Get "topstories" Section state for default values
    const topStories = this.props.Sections.find(s => s.id === "topstories");

    if (!topStories) {
      return null;
    }

    // Extract TopSites to render before the rest and Message to use for header
    const topSites = extractComponent("TopSites");
    const sponsoredCollection = extractComponent("CollectionCardGrid");
    const message = extractComponent("Message") || {
      header: {
        link_text: topStories.learnMore.link.message,
        link_url: topStories.learnMore.link.href,
        title: topStories.title,
      },
    };

    // Render a DS-style TopSites then the rest if any in a collapsible section
    return (
      <React.Fragment>
        {this.props.DiscoveryStream.isPrivacyInfoModalVisible && (
          <DSPrivacyModal dispatch={this.props.dispatch} />
        )}
        {topSites &&
          this.renderLayout([
            {
              width: 12,
              components: [topSites],
            },
          ])}
        {sponsoredCollection &&
          this.renderLayout([
            {
              width: 12,
              components: [sponsoredCollection],
            },
          ])}
        {!!layoutRender.length && (
          <CollapsibleSection
            className="ds-layout"
            collapsed={topStories.pref.collapsed}
            dispatch={this.props.dispatch}
            icon={topStories.icon}
            id={topStories.id}
            isFixed={true}
            learnMore={{
              link: {
                href: message.header.link_url,
                message: message.header.link_text,
              },
            }}
            privacyNoticeURL={topStories.privacyNoticeURL}
            showPrefName={topStories.pref.feed}
            title={message.header.title}
          >
            {this.renderLayout(layoutRender)}
          </CollapsibleSection>
        )}
        {this.renderLayout([
          {
            width: 12,
            components: [{ type: "Highlights" }],
          },
        ])}
      </React.Fragment>
    );
  }

  renderLayout(layoutRender) {
    const styles = [];
    return (
      <div className="discovery-stream ds-layout">
        {layoutRender.map((row, rowIndex) => (
          <div
            key={`row-${rowIndex}`}
            className={`ds-column ds-column-${row.width}`}
          >
            <div className="ds-column-grid">
              {row.components.map((component, componentIndex) => {
                if (!component) {
                  return null;
                }
                styles[rowIndex] = [
                  ...(styles[rowIndex] || []),
                  component.styles,
                ];
                return (
                  <div key={`component-${componentIndex}`}>
                    {this.renderComponent(component, row.width)}
                  </div>
                );
              })}
            </div>
          </div>
        ))}
        {this.renderStyles(styles)}
      </div>
    );
  }
}

export const DiscoveryStreamBase = connect(state => ({
  DiscoveryStream: state.DiscoveryStream,
  Prefs: state.Prefs,
  Sections: state.Sections,
  document: global.document,
}))(_DiscoveryStreamBase);

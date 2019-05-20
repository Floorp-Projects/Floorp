import {Localized} from "fluent-react";
import React from "react";
import {RICH_TEXT_KEYS} from "../../rich-text-strings";
import {safeURI} from "../../template-utils";

// Elements allowed in snippet content
const ALLOWED_TAGS = {
  b: <b />,
  i: <i />,
  u: <u />,
  strong: <strong />,
  em: <em />,
  br: <br />,
};

/**
 * Transform an object (tag name: {url}) into (tag name: anchor) where the url
 * is used as href, in order to render links inside a Fluent.Localized component.
 */
export function convertLinks(links, sendClick, doNotAutoBlock, openNewWindow = false) {
  if (links) {
    return Object.keys(links).reduce((acc, linkTag) => {
      const {action} = links[linkTag];
      // Setting the value to false will not include the attribute in the anchor
      const url = action ? false : safeURI(links[linkTag].url);

      acc[linkTag] = (<a href={url} // eslint-disable-line jsx-a11y/anchor-has-content
        // eslint was getting a false positive caused by the dynamic injection of content.
        target={openNewWindow ? "_blank" : ""}
        data-metric={links[linkTag].metric}
        data-action={action}
        data-args={links[linkTag].args}
        data-do_not_autoblock={doNotAutoBlock}
        onClick={sendClick} />);
      return acc;
    }, {});
  }

  return null;
}

/**
 * Message wrapper used to sanitize markup and render HTML.
 */
export function RichText(props) {
  if (!RICH_TEXT_KEYS.includes(props.localization_id)) {
    throw new Error(`ASRouter: ${props.localization_id} is not a valid rich text property. If you want it to be processed, you need to add it to asrouter/rich-text-strings.js`);
  }
  return (
    <Localized id={props.localization_id} {...ALLOWED_TAGS} {...props.customElements} {...convertLinks(props.links, props.sendClick, props.doNotAutoBlock, props.openNewWindow)}>
      <span>{props.text}</span>
    </Localized>
  );
}

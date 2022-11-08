/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const XHTML_NS = "http://www.w3.org/1999/xhtml";

class CssQueryContainerTooltipHelper {
  /**
   * Fill the tooltip with container information.
   */
  async setContent(data, tooltip) {
    const res = await data.rule.domRule.getQueryContainerForNode(
      data.ancestorIndex,
      data.rule.inherited ||
        data.rule.elementStyle.ruleView.inspector.selection.nodeFront
    );

    const fragment = this.#getTemplate(res, tooltip);
    tooltip.panel.innerHTML = "";
    tooltip.panel.appendChild(fragment);

    // Size the content.
    tooltip.setContentSize({ width: 267, height: Infinity });
  }

  /**
   * Get the template of the tooltip.
   *
   * @param {Object} data
   * @param {NodeFront} data.node
   * @param {string} data.containerType
   * @param {string} data.inlineSize
   * @param {string} data.blockSize
   * @param {HTMLTooltip} tooltip
   *        The tooltip we are targetting.
   */
  #getTemplate(data, tooltip) {
    const { doc } = tooltip;

    const templateNode = doc.createElementNS(XHTML_NS, "template");

    const tooltipContainer = doc.createElementNS(XHTML_NS, "div");
    tooltipContainer.classList.add("devtools-tooltip-query-container");
    templateNode.content.appendChild(tooltipContainer);

    const nodeContainer = doc.createElementNS(XHTML_NS, "header");
    tooltipContainer.append(nodeContainer);

    const containerQueryLabel = doc.createElementNS(XHTML_NS, "span");
    containerQueryLabel.classList.add("property-name");
    containerQueryLabel.appendChild(doc.createTextNode(`query container`));

    const nodeEl = doc.createElementNS(XHTML_NS, "span");
    nodeEl.classList.add("objectBox-node");
    nodeContainer.append(doc.createTextNode("<"), nodeEl);

    const nodeNameEl = doc.createElementNS(XHTML_NS, "span");
    nodeNameEl.classList.add("tag-name");
    nodeNameEl.appendChild(
      doc.createTextNode(data.node.nodeName.toLowerCase())
    );

    nodeEl.appendChild(nodeNameEl);

    if (data.node.id) {
      const idEl = doc.createElementNS(XHTML_NS, "span");
      idEl.classList.add("attribute-name");
      idEl.appendChild(doc.createTextNode(`#${data.node.id}`));
      nodeEl.appendChild(idEl);
    }

    for (const attr of data.node.attributes) {
      if (attr.name !== "class") {
        continue;
      }
      for (const cls of attr.value.split(/\s/)) {
        const el = doc.createElementNS(XHTML_NS, "span");
        el.classList.add("attribute-name");
        el.appendChild(doc.createTextNode(`.${cls}`));
        nodeEl.appendChild(el);
      }
    }
    nodeContainer.append(doc.createTextNode(">"));

    const ul = doc.createElementNS(XHTML_NS, "ul");
    tooltipContainer.appendChild(ul);

    const containerTypeEl = doc.createElementNS(XHTML_NS, "li");
    const containerTypeLabel = doc.createElementNS(XHTML_NS, "span");
    containerTypeLabel.classList.add("property-name");
    containerTypeLabel.appendChild(doc.createTextNode(`container-type`));

    const containerTypeValue = doc.createElementNS(XHTML_NS, "span");
    containerTypeValue.classList.add("property-value");
    containerTypeValue.appendChild(doc.createTextNode(data.containerType));

    containerTypeEl.append(
      containerTypeLabel,
      doc.createTextNode(": "),
      containerTypeValue
    );
    ul.appendChild(containerTypeEl);

    const inlineSizeEl = doc.createElementNS(XHTML_NS, "li");

    const inlineSizeLabel = doc.createElementNS(XHTML_NS, "span");
    inlineSizeLabel.classList.add("property-name");
    inlineSizeLabel.appendChild(doc.createTextNode(`inline-size`));

    const inlineSizeValue = doc.createElementNS(XHTML_NS, "span");
    inlineSizeValue.classList.add("property-value");
    inlineSizeValue.appendChild(doc.createTextNode(data.inlineSize));

    inlineSizeEl.append(
      inlineSizeLabel,
      doc.createTextNode(": "),
      inlineSizeValue
    );
    ul.appendChild(inlineSizeEl);

    if (data.containerType != "inline-size") {
      const blockSizeEl = doc.createElementNS(XHTML_NS, "li");
      const blockSizeLabel = doc.createElementNS(XHTML_NS, "span");
      blockSizeLabel.classList.add("property-name");
      blockSizeLabel.appendChild(doc.createTextNode(`block-size`));

      const blockSizeValue = doc.createElementNS(XHTML_NS, "span");
      blockSizeValue.classList.add("property-value");
      blockSizeValue.appendChild(doc.createTextNode(data.blockSize));

      blockSizeEl.append(
        blockSizeLabel,
        doc.createTextNode(": "),
        blockSizeValue
      );
      ul.appendChild(blockSizeEl);
    }

    return doc.importNode(templateNode.content, true);
  }
}

module.exports = CssQueryContainerTooltipHelper;

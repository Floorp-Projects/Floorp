/*
 license: The MIT License, Copyright (c) 2020 YUKI "Piro" Hiroshi
*/

import { SequenceMatcher } from './diff.js';

export const DOMUpdater = {
  /**
   * method
   * @param before {Node}   - the node to be updated, e.g. Element
   * @param after  {Node}   - the node describing updated state,
   *                          e.g. DocumentFragment
   * @return count {number} - the count of appied changes
   */
  update(before, after, context) {
    let topLevel = false;
    if (!context) {
      topLevel = true;
      context = {
        count:       0,
        beforeRange: before.ownerDocument.createRange(),
        afterRange:  after.ownerDocument.createRange()
      };
    }
    const { beforeRange, afterRange } = context;

    if (before.nodeValue !== null ||
        after.nodeValue !== null) {
      if (before.nodeValue != after.nodeValue) {
        //console.log('node value: ', after.nodeValue);
        before.nodeValue = after.nodeValue;
        context.count++;
      }
      return context.count;
    }

    const beforeNodes = Array.from(before.childNodes, this._getDiffableNodeString);
    const afterNodes = Array.from(after.childNodes, this._getDiffableNodeString);
    const nodeOerations = (new SequenceMatcher(beforeNodes, afterNodes)).operations();
    // Update from back to front for safety!
    for (const operation of nodeOerations.reverse()) {
      const [tag, fromStart, fromEnd, toStart, toEnd] = operation;
      switch (tag) {
        case 'equal':
          for (let i = 0, maxi = fromEnd - fromStart; i < maxi; i++) {
            this.update(
              before.childNodes[fromStart + i],
              after.childNodes[toStart + i],
              context
            );
          }
          break;
        case 'delete':
          beforeRange.setStart(before, fromStart);
          beforeRange.setEnd(before, fromEnd);
          beforeRange.deleteContents();
          context.count++;
          break;
        case 'insert':
          beforeRange.setStart(before, fromStart);
          beforeRange.setEnd(before, fromEnd);
          afterRange.setStart(after, toStart);
          afterRange.setEnd(after, toEnd);
          beforeRange.insertNode(afterRange.cloneContents());
          context.count++;
          break;
        case 'replace':
          beforeRange.setStart(before, fromStart);
          beforeRange.setEnd(before, fromEnd);
          beforeRange.deleteContents();
          context.count++;
          afterRange.setStart(after, toStart);
          afterRange.setEnd(after, toEnd);
          beforeRange.insertNode(afterRange.cloneContents());
          context.count++;
          break;
      }
    }
    if (topLevel) {
      beforeRange.detach();
      afterRange.detach();
    }

    if (before.nodeType == before.ELEMENT_NODE &&
        after.nodeType == after.ELEMENT_NODE) {
      const beforeAttrs = Array.from(before.attributes, attr => `${attr.name}:${attr.value}`).  sort();
      const afterAttrs = Array.from(after.attributes, attr => `${attr.name}:${attr.value}`).  sort();
      const attrOerations = (new SequenceMatcher(beforeAttrs, afterAttrs)).operations();
      for (const operation of attrOerations) {
        const [tag, fromStart, fromEnd, toStart, toEnd] = operation;
        switch (tag) {
          case 'equal':
            break;
          case 'delete':
            for (let i = fromStart; i < fromEnd; i++) {
              const name = beforeAttrs[i].split(':')[0];
              //console.log('delete: delete attr: ', name);
              before.removeAttribute(name);
              context.count++;
            }
            break;
          case 'insert':
            for (let i = toStart; i < toEnd; i++) {
              const attr = afterAttrs[i].split(':');
              const name = attr[0];
              const value = attr.slice(1).join(':');
              //console.log('insert: set attr: ', name, value);
              before.setAttribute(name, value);
              context.count++;
            }
            break;
          case 'replace':
            const insertedAttrs = new Set();
            for (let i = toStart; i < toEnd; i++) {
              const attr = afterAttrs[i].split(':');
              const name = attr[0];
              const value = attr.slice(1).join(':');
              //console.log('replace: set attr: ', name, value);
              before.setAttribute(name, value);
              insertedAttrs.add(name);
              context.count++;
            }
            for (let i = fromStart; i < fromEnd; i++) {
              const name = beforeAttrs[i].split(':')[0];
              if (insertedAttrs.has(name))
                continue;
              //console.log('replace: delete attr: ', name);
              before.removeAttribute(name);
              context.count++;
            }
            break;
        }
      }
    }
    return context.count;
  },

  _getDiffableNodeString(node) {
    if (node.nodeType == node.ELEMENT_NODE)
      return `element:${node.tagName}#${node.id}#${node.getAttribute('anonid')}`;
    else
      return `node:${node.nodeType}`;
  }

};

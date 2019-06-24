/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
/* eslint camelcase: 0*/

import type { OriginalFrame, SourceLocation, SourceId } from "debugger-html";
import type { Expr } from "./wasmDwarfExpressions";

const { decodeExpr } = require("./wasmDwarfExpressions");

const xScopes = new Map();

type XScopeItem = any;
type XScopeItemsIndex = Map<string | number, XScopeItem>;

type XScopeVariable = {
  name: string,
  expr?: Expr,
};

type XScopeVariables = {
  vars: XScopeVariable[],
  frameBase?: Expr | null,
};

function indexLinkingNames(items: XScopeItem[]): XScopeItemsIndex {
  const result = new Map();
  let queue = [...items];
  while (queue.length > 0) {
    const item = queue.shift();
    if ("uid" in item) {
      result.set(item.uid, item);
    } else if ("linkage_name" in item) {
      // TODO the linkage_name string value is used for compatibility
      // with old format. Remove in favour of the uid referencing.
      result.set(item.linkage_name, item);
    }
    if ("children" in item) {
      queue = [...queue, ...item.children];
    }
  }
  return result;
}

function getIndexedItem(
  index: XScopeItemsIndex,
  key: string | { uid: number }
): XScopeItem {
  if (typeof key === "object" && key != null) {
    return index.get(key.uid);
  }
  if (typeof key === "string") {
    return index.get(key);
  }
  return null;
}

type XScopeData = {
  code_section_offset: number,
  debug_info: Array<XScopeItem>,
  idIndex: XScopeItemsIndex,
  sources: Array<string>,
};

async function getXScopes(
  sourceId: SourceId,
  getSourceMap: (
    sourceId: SourceId
  ) => {
    sources: string[],
    xScopes?: XScopeData,
  }
): Promise<?XScopeData> {
  if (xScopes.has(sourceId)) {
    return xScopes.get(sourceId);
  }
  const map = await getSourceMap(sourceId);
  if (!map || !map.xScopes) {
    xScopes.set(sourceId, null);
    return null;
  }
  const { code_section_offset, debug_info } = map.xScopes;
  const xScope = {
    code_section_offset,
    debug_info,
    idIndex: indexLinkingNames(debug_info),
    sources: map.sources,
  };
  xScopes.set(sourceId, xScope);
  return xScope;
}

function isInRange(item: XScopeItem, pc: number): boolean {
  if ("ranges" in item) {
    return item.ranges.some(r => r[0] <= pc && pc < r[1]);
  }
  if ("high_pc" in item) {
    return item.low_pc <= pc && pc < item.high_pc;
  }
  return false;
}

type FoundScope = {
  id: string,
  name?: string,
  variables: XScopeVariables,
  file?: number,
  line?: number,
};

type EncodedExpr =
  | string
  | Array<{
      expr: string,
      range: number[],
    }>;

function decodeExprAt(expr: EncodedExpr, pc: number): ?Expr {
  if (typeof expr === "string") {
    return decodeExpr(expr);
  }
  const foundAt = expr.find(i => i.range[0] <= pc && pc < i.range[1]);
  return foundAt ? decodeExpr(foundAt.expr) : null;
}

function getVariables(scope: XScopeItem, pc: number): XScopeVariables {
  const vars = scope.children
    ? scope.children.reduce((result, item) => {
        switch (item.tag) {
          case "variable":
          case "formal_parameter":
            result.push({
              name: item.name || "",
              expr: item.location ? decodeExprAt(item.location, pc) : null,
            });
            break;
          case "lexical_block":
            // FIXME build scope blocks (instead of combining)
            const tmp = getVariables(item, pc);
            result = [...tmp.vars, ...result];
            break;
        }
        return result;
      }, [])
    : [];
  const frameBase = scope.frame_base ? decodeExpr(scope.frame_base) : null;
  return {
    vars,
    frameBase,
  };
}

function filterScopes(
  items: XScopeItem[],
  pc: number,
  lastItem: ?FoundScope,
  index: XScopeItemsIndex
): FoundScope[] {
  if (!items) {
    return [];
  }
  return items.reduce((result, item) => {
    switch (item.tag) {
      case "compile_unit":
        if (isInRange(item, pc)) {
          result = [
            ...result,
            ...filterScopes(item.children, pc, lastItem, index),
          ];
        }
        break;
      case "namespace":
      case "structure_type":
      case "union_type":
        result = [
          ...result,
          ...filterScopes(item.children, pc, lastItem, index),
        ];
        break;
      case "subprogram":
        if (isInRange(item, pc)) {
          const s: FoundScope = {
            id: item.linkage_name,
            name: item.name,
            variables: getVariables(item, pc),
          };
          result = [...result, s, ...filterScopes(item.children, pc, s, index)];
        }
        break;
      case "inlined_subroutine":
        if (isInRange(item, pc)) {
          const linkedItem = getIndexedItem(index, item.abstract_origin);
          const s: FoundScope = {
            id: item.abstract_origin,
            name: linkedItem ? linkedItem.name : void 0,
            variables: getVariables(item, pc),
          };
          if (lastItem) {
            lastItem.file = item.call_file;
            lastItem.line = item.call_line;
          }
          result = [...result, s, ...filterScopes(item.children, pc, s, index)];
        }
        break;
    }
    return result;
  }, []);
}

interface XScopeSourceMapContext {
  getSourceMap(sourceId: SourceId): any;
  generatedToOriginalId(sourceId: SourceId, url: string): SourceId;
}

class XScope {
  xScope: XScopeData;
  sourceMapContext: XScopeSourceMapContext;

  constructor(
    xScopeData: XScopeData,
    sourceMapContext: XScopeSourceMapContext
  ) {
    this.xScope = xScopeData;
    this.sourceMapContext = sourceMapContext;
  }

  search(generatedLocation: SourceLocation): Array<OriginalFrame> {
    const { code_section_offset, debug_info, sources, idIndex } = this.xScope;
    const pc = generatedLocation.line - (code_section_offset || 0);
    const scopes = filterScopes(debug_info, pc, null, idIndex);
    scopes.reverse();

    return scopes.map(i => {
      if (!("file" in i)) {
        return {
          displayName: i.name || "",
          variables: i.variables,
        };
      }
      const sourceId = this.sourceMapContext.generatedToOriginalId(
        generatedLocation.sourceId,
        sources[i.file || 0]
      );
      return {
        displayName: i.name || "",
        variables: i.variables,
        location: {
          line: i.line || 0,
          sourceId,
        },
      };
    });
  }
}

async function getWasmXScopes(
  sourceId: SourceId,
  sourceMapContext: XScopeSourceMapContext
): Promise<?XScope> {
  const { getSourceMap } = sourceMapContext;
  const xScopeData = await getXScopes(sourceId, getSourceMap);
  if (!xScopeData) {
    return null;
  }
  return new XScope(xScopeData, sourceMapContext);
}

function clearWasmXScopes() {
  xScopes.clear();
}

module.exports = {
  getWasmXScopes,
  clearWasmXScopes,
};

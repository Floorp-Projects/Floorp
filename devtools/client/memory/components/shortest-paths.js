/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  DOM: dom,
  createClass,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { isSavedFrame } = require("devtools/shared/DevToolsUtils");
const { getSourceNames } = require("devtools/client/shared/source-utils");
const { L10N } = require("../utils");

const GRAPH_DEFAULTS = {
  translate: [20, 20],
  scale: 1
};

const NO_STACK = "noStack";
const NO_FILENAME = "noFilename";
const ROOT_LIST = "JS::ubi::RootList";

function stringifyLabel(label, id) {
  const sanitized = [];

  for (let i = 0, length = label.length; i < length; i++) {
    const piece = label[i];

    if (isSavedFrame(piece)) {
      const { short } = getSourceNames(piece.source);
      sanitized[i] = `${piece.functionDisplayName} @ ${short}:${piece.line}:${piece.column}`;
    } else if (piece === NO_STACK) {
      sanitized[i] = L10N.getStr("tree-item.nostack");
    } else if (piece === NO_FILENAME) {
      sanitized[i] = L10N.getStr("tree-item.nofilename");
    } else if (piece === ROOT_LIST) {
      // Don't use the usual labeling machinery for root lists: replace it
      // with the "GC Roots" string.
      sanitized.splice(0, label.length);
      sanitized.push(L10N.getStr("tree-item.rootlist"));
      break;
    } else {
      sanitized[i] = "" + piece;
    }
  }

  return `${sanitized.join(" › ")} @ 0x${id.toString(16)}`;
}

module.exports = createClass({
  displayName: "ShortestPaths",

  propTypes: {
    graph: PropTypes.shape({
      nodes: PropTypes.arrayOf(PropTypes.object),
      edges: PropTypes.arrayOf(PropTypes.object),
    }),
  },

  getInitialState() {
    return { zoom: null };
  },

  shouldComponentUpdate(nextProps) {
    return this.props.graph != nextProps.graph;
  },

  componentDidMount() {
    if (this.props.graph) {
      this._renderGraph(this.refs.container, this.props.graph);
    }
  },

  componentDidUpdate() {
    if (this.props.graph) {
      this._renderGraph(this.refs.container, this.props.graph);
    }
  },

  componentWillUnmount() {
    if (this.state.zoom) {
      this.state.zoom.on("zoom", null);
    }
  },

  render() {
    let contents;
    if (this.props.graph) {
      // Let the componentDidMount or componentDidUpdate method draw the graph
      // with DagreD3. We just provide the container for the graph here.
      contents = dom.div({
        ref: "container",
        style: {
          flex: 1,
          height: "100%",
          width: "100%",
        }
      });
    } else {
      contents = dom.div(
        {
          id: "shortest-paths-select-node-msg"
        },
        L10N.getStr("shortest-paths.select-node")
      );
    }

    return dom.div(
      {
        id: "shortest-paths",
        className: "vbox",
      },
      dom.label(
        {
          id: "shortest-paths-header",
          className: "header",
        },
        L10N.getStr("shortest-paths.header")
      ),
      contents
    );
  },

  _renderGraph(container, { nodes, edges }) {
    if (!container.firstChild) {
      const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
      svg.setAttribute("id", "graph-svg");
      svg.setAttribute("xlink", "http://www.w3.org/1999/xlink");
      svg.style.width = "100%";
      svg.style.height = "100%";

      const target = document.createElementNS("http://www.w3.org/2000/svg", "g");
      target.setAttribute("id", "graph-target");
      target.style.width = "100%";
      target.style.height = "100%";

      svg.appendChild(target);
      container.appendChild(svg);
    }

    const graph = new dagreD3.Digraph();

    for (let i = 0; i < nodes.length; i++) {
      graph.addNode(nodes[i].id, {
        id: nodes[i].id,
        label: stringifyLabel(nodes[i].label, nodes[i].id),
      });
    }

    for (let i = 0; i < edges.length; i++) {
      graph.addEdge(null, edges[i].from, edges[i].to, {
        label: edges[i].name
      });
    }

    const renderer = new dagreD3.Renderer();
    renderer.drawNodes();
    renderer.drawEdgePaths();

    const svg = d3.select("#graph-svg");
    const target = d3.select("#graph-target");

    let zoom = this.state.zoom;
    if (!zoom) {
      zoom = d3.behavior.zoom().on("zoom", function () {
        target.attr(
          "transform",
          `translate(${d3.event.translate}) scale(${d3.event.scale})`
        );
      });
      svg.call(zoom);
      this.setState({ zoom });
    }

    const { translate, scale } = GRAPH_DEFAULTS;
    zoom.scale(scale);
    zoom.translate(translate);
    target.attr("transform", `translate(${translate}) scale(${scale})`);

    const layout = dagreD3.layout();
    renderer.layout(layout).run(graph, target);
  },
});

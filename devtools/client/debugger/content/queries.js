
function getSource(state, actor) {
  return state.sources.sources[actor];
}

function getSources(state) {
  return state.sources.sources;
}

function getSourceCount(state) {
  return Object.keys(state.sources.sources).length;
}

function getSourceByURL(state, url) {
  for (let k in state.sources.sources) {
    const source = state.sources.sources[k];
    if (source.url === url) {
      return source;
    }
  }
}

function getSourceByActor(state, actor) {
  for (let k in state.sources.sources) {
    const source = state.sources.sources[k];
    if (source.actor === actor) {
      return source;
    }
  }
}

function getSelectedSource(state) {
  return state.sources.sources[state.sources.selectedSource];
}

function getSelectedSourceOpts(state) {
  return state.sources.selectedSourceOpts;
}

function getSourceText(state, actor) {
  return state.sources.sourcesText[actor];
}

function getBreakpoints(state) {
  return Object.keys(state.breakpoints.breakpoints).map(k => {
    return state.breakpoints.breakpoints[k];
  });
}

function getBreakpoint(state, location) {
  return state.breakpoints.breakpoints[makeLocationId(location)];
}

function makeLocationId(location) {
  return location.actor + ":" + location.line.toString();
}

module.exports = {
  getSource,
  getSources,
  getSourceCount,
  getSourceByURL,
  getSourceByActor,
  getSelectedSource,
  getSelectedSourceOpts,
  getSourceText,
  getBreakpoint,
  getBreakpoints,
  makeLocationId
};

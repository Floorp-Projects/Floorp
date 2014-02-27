// Bug 977853 -- Pared down version of script exhibiting negative
// interaction with convert to doubles optimization. See bug for gory
// details.

if (!getBuildConfiguration().parallelJS)
  quit();

load(libdir + "parallelarray-helpers.js")

var numIters = 5;
var golden_output;

function PJS_div4(v, s)
{
  return [ v[0]/s, v[1]/s, v[2]/s, v[3]/s ];
}

function PJS_normalized(v)
{
  var d = Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  d = d > 0.0 ? d : 1.0;
  var result = [ v[0]/d, v[1]/d, v[2]/d, 1.0 ];
  return result;
}

// This is the elemental function passed to mapPar
function PJS_displace(p)
{
  var position = [p[0], p[1], p[2], 1.0];
  var normal = position;
  var roughness = 0.025 / 0.35;
  normal = PJS_normalized(PJS_div4(normal, roughness));
  return null;
}
var NUM_VERTEX_COMPONENTS = 3;
var initPos, nVertices;
var userData = {
  nVertices : 25, //2880,
  initPos : [],
};
function setup() {
  for(var k = 0; k < NUM_VERTEX_COMPONENTS*userData.nVertices; k++) {
    userData.initPos[k] = k/1000;
  }
  nVertices	= userData.nVertices;
  initPos		= new Array(nVertices);
  for(var i=0, j=0; i<nVertices; i++, j+=NUM_VERTEX_COMPONENTS) {
	initPos[i] = [userData.initPos[j],
				  userData.initPos[j+1],
				  userData.initPos[j+2]];
  }
}
function SimulatePJS() {
  var curPosAndNor;

  // Measure Parallel Execution
  assertParallelExecSucceeds(
    function(m) { return initPos.mapPar(PJS_displace, m); },
    function() { });
}
var start_time, elapsed_parallel = 0, elapsed_sequential = 0;
setup();
SimulatePJS();

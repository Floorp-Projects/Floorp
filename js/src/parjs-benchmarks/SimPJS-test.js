// This code was written by Intel.
// Its copyright status is unclear but it was attached by Intel
// employees to a bugzilla bug.

if (!getBuildConfiguration().parallelJS)
  quit(0);

// To adjust the time taken by this program, change numIters or nVertices

var numIters = 20;
var golden_output;
var PJS_P_MASK = 255;
var PJS_P_SIZE = 256;
var PJS_P = [151,160,137,91,90,15,
  131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
  190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
  88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
  77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
  102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
  135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
  5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
  223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
  129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
  251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
  49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
  151,160,137,91,90,15,
  131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
  190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
  88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
  77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
  102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
  135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
  5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
  223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
  129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
  251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
  49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
  138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
  ];

var PJS_G_MASK = 15;
var PJS_G_SIZE = 16;
var PJS_G_VECSIZE = 4;
var PJS_G = [
	 +1.0, +1.0, +0.0, 0.0 ,
	 -1.0, +1.0, +0.0, 0.0 ,
	 +1.0, -1.0, +0.0, 0.0 ,
	 -1.0, -1.0, +0.0, 0.0 ,
	 +1.0, +0.0, +1.0, 0.0 ,
	 -1.0, +0.0, +1.0, 0.0 ,
	 +1.0, +0.0, -1.0, 0.0 ,
	 -1.0, +0.0, -1.0, 0.0 ,
	 +0.0, +1.0, +1.0, 0.0 ,
	 +0.0, -1.0, +1.0, 0.0 ,
	 +0.0, +1.0, -1.0, 0.0 ,
	 +0.0, -1.0, -1.0, 0.0 ,
	 +1.0, +1.0, +0.0, 0.0 ,
	 -1.0, +1.0, +0.0, 0.0 ,
	 +0.0, -1.0, +1.0, 0.0 ,
	 +0.0, -1.0, -1.0, 0.0
];

function PJS_add4(a, b)
{
	return [ a[0]+b[0], a[1]+b[1], a[2]+b[2], a[3]+b[3] ];
}

function PJS_sub4(a, b)
{
	return [ a[0]-b[0], a[1]-b[1], a[2]-b[2], a[3]-b[3] ];
}

function PJS_mul4(v, s)
{
	return [ s*v[0], s*v[1], s*v[2], s*v[3] ];
}

function PJS_div4(v, s)
{
	return [ v[0]/s, v[1]/s, v[2]/s, v[3]/s ];
}

function PJS_dot4(a, b)
{
	return (a[0]*b[0]) + (a[1]*b[1]) + (a[2]*b[2]) + (a[3]*b[3]);
}

function PJS_clamp(x, minval, maxval)
{
	var mx = (x > minval) ? x : minval;
	return (mx < maxval) ? mx : maxval;
}

function PJS_mix1d(a, b, t)
{
	var ba = b - a;
	var tba = t * ba;
	var atba = a + tba;
	return atba;
}

function PJS_mix2d(a, b, t)
{
	var ba   = [0, 0];
	var tba  = [0, 0];
	var atba = [0, 0];
	ba[0] = b[0] - a[0];
	ba[1] = b[1] - a[1];
	tba[0] = t * ba[0];
	tba[1] = t * ba[1];
	atba[0] = a[0] + tba[0];
	atba[1] = a[1] + tba[1];
	return atba;
}

function PJS_mix3d(a, b, t)
{
	var ba   = [0, 0, 0, 0];
	var tba  = [0, 0, 0, 0];
	var atba = [0, 0, 0, 0];
	ba[0] = b[0] - a[0];
	ba[1] = b[1] - a[1];
	ba[2] = b[2] - a[2];
	ba[3] = b[3] - a[3];
	tba[0] = t * ba[0];
	tba[1] = t * ba[1];
	tba[2] = t * ba[2];
	tba[3] = t * ba[3];
	atba[0] = a[0] + tba[0];
	atba[1] = a[1] + tba[1];
	atba[2] = a[2] + tba[2];
	atba[3] = a[3] + tba[3];
	return atba;
}

function PJS_smooth(t)
{
	return t*t*t*(t*(t*6.0-15.0)+10.0);
}

function PJS_lattice3d(i)
{
	return PJS_P[i[0] + PJS_P[i[1] + PJS_P[i[2]]]];
}

function PJS_gradient3d(i, v)
{
	var index = (PJS_lattice3d(i) & PJS_G_MASK) * PJS_G_VECSIZE;
	var g = [ PJS_G[index + 0], PJS_G[index + 1], PJS_G[index + 2], 1.0 ];
	return PJS_dot4(v, g);
}

function PJS_normalized(v)
{
	var d = Math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	d = d > 0.0 ? d : 1.0;
	var result = [ v[0]/d, v[1]/d, v[2]/d, 1.0 ];
	return result;
}

function PJS_gradient_noise3d(position)
{

	var p = position;
	var pf = [ Math.floor(p[0]), Math.floor(p[1]), Math.floor(p[2]), Math.floor(p[3]) ];
	var ip = [ pf[0], pf[1], pf[2], 0 ];
	var fp = PJS_sub4(p, pf);

	ip[0] = ip[0] & PJS_P_MASK;
	ip[1] = ip[1] & PJS_P_MASK;
	ip[2] = ip[2] & PJS_P_MASK;
	ip[3] = ip[3] & PJS_P_MASK;

	var I000 = [0, 0, 0, 0];
	var I001 = [0, 0, 1, 0];
	var I010 = [0, 1, 0, 0];
	var I011 = [0, 1, 1, 0];
	var I100 = [1, 0, 0, 0];
	var I101 = [1, 0, 1, 0];
	var I110 = [1, 1, 0, 0];
	var I111 = [1, 1, 1, 0];

	var F000 = [0.0, 0.0, 0.0, 0.0];
	var F001 = [0.0, 0.0, 1.0, 0.0];
	var F010 = [0.0, 1.0, 0.0, 0.0];
	var F011 = [0.0, 1.0, 1.0, 0.0];
	var F100 = [1.0, 0.0, 0.0, 0.0];
	var F101 = [1.0, 0.0, 1.0, 0.0];
	var F110 = [1.0, 1.0, 0.0, 0.0];
	var F111 = [1.0, 1.0, 1.0, 0.0];

	var n000 = PJS_gradient3d(PJS_add4(ip, I000), PJS_sub4(fp, F000));
	var n001 = PJS_gradient3d(PJS_add4(ip, I001), PJS_sub4(fp, F001));

	var n010 = PJS_gradient3d(PJS_add4(ip, I010), PJS_sub4(fp, F010));
	var n011 = PJS_gradient3d(PJS_add4(ip, I011), PJS_sub4(fp, F011));

	var n100 = PJS_gradient3d(PJS_add4(ip, I100), PJS_sub4(fp, F100));
	var n101 = PJS_gradient3d(PJS_add4(ip, I101), PJS_sub4(fp, F101));

	var n110 = PJS_gradient3d(PJS_add4(ip, I110), PJS_sub4(fp, F110));
	var n111 = PJS_gradient3d(PJS_add4(ip, I111), PJS_sub4(fp, F111));

	var n40 = [n000, n001, n010, n011];
	var n41 = [n100, n101, n110, n111];

	var n4 = PJS_mix3d(n40, n41, PJS_smooth(fp[0]));
	var n2 = PJS_mix2d([n4[0], n4[1]], [n4[2], n4[3]], PJS_smooth(fp[1]));
	var n = 0.5 - 0.5 * PJS_mix1d(n2[0], n2[1], PJS_smooth(fp[2]));
	return n;
}

function PJS_ridgedmultifractal3d(
	position,
	frequency,
	lacunarity,
	increment,
	octaves)
{
	var i = 0;
	var fi = 0.0;
	var remainder = 0.0;
	var sample = 0.0;
	var value = 0.0;
	var iterations = Math.floor(octaves);

	var threshold = 0.5;
	var offset = 1.0;
	var weight = 1.0;

	var signal = Math.abs( (1.0 - 2.0 * PJS_gradient_noise3d(PJS_mul4(position, frequency))) );
	signal = offset - signal;
	signal *= signal;
	value = signal;

	for ( i = 0; i < iterations; i++ )
	{
		frequency *= lacunarity;
		weight = PJS_clamp( signal * threshold, 0.0, 1.0 );
		signal = Math.abs( (1.0 - 2.0 * PJS_gradient_noise3d(PJS_mul4(position, frequency))) );
		signal = offset - signal;
		signal *= signal;
		signal *= weight;
		value += signal * Math.pow( lacunarity, -fi * increment );
	}
	return value;
}

var PJS_frequency;
var PJS_amplitude;
var PJS_phase;
var PJS_lacunarity;
var PJS_increment;
var PJS_octaves;
var PJS_roughness;

// This is the elemental function passed to mapPar
function PJS_displace(p)
{
	var position = [p[0], p[1], p[2], 1.0];
	var normal = position;

	var roughness = PJS_roughness / PJS_amplitude;
	var sample = PJS_add4(position, [PJS_phase + 100.0, PJS_phase + 100.0, PJS_phase + 100.0, 0.0]);

	var dx = [roughness, 0.0, 0.0, 1.0];
	var dy = [0.0, roughness, 0.0, 1.0];
	var dz = [0.0, 0.0, roughness, 1.0];

	var f0 = PJS_ridgedmultifractal3d(sample, PJS_frequency, PJS_lacunarity, PJS_increment, PJS_octaves);
	var f1 = PJS_ridgedmultifractal3d(PJS_add4(sample, dx), PJS_frequency, PJS_lacunarity, PJS_increment, PJS_octaves);
	var f2 = PJS_ridgedmultifractal3d(PJS_add4(sample, dy), PJS_frequency, PJS_lacunarity, PJS_increment, PJS_octaves);
	var f3 = PJS_ridgedmultifractal3d(PJS_add4(sample, dz), PJS_frequency, PJS_lacunarity, PJS_increment, PJS_octaves);

	var displacement = (f0 + f1 + f2 + f3) / 4.0;

	var vertex = PJS_add4(position, PJS_mul4(normal, PJS_amplitude * displacement));
	vertex[3] = 1.0;

	normal[0] -= (f1 - f0);
	normal[1] -= (f2 - f0);
	normal[2] -= (f3 - f0);
	normal = PJS_normalized(PJS_div4(normal, roughness));

	return {pos : [vertex[0], vertex[1], vertex[2]], nor : [normal[0], normal[1], normal[2]]};
}
var NUM_VERTEX_COMPONENTS = 3;
var initPos, nVertices;
var userData = {
    nVertices : 2880,
    initPos : [],
    frequency : 1.0,
    amplitude : 0.35,
    phase : 0.0,
    lacunarity : 2.0,
    increment : 1.5,
    octaves : 5.5,
    roughness : 0.025,
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
function SimulatePJS(runInParallel) {
    var curPosAndNor;

	PJS_frequency	= userData.frequency;
	PJS_amplitude	= userData.amplitude;
	PJS_phase		= userData.phase;
	PJS_lacunarity	= userData.lacunarity;
	PJS_increment	= userData.increment;
	PJS_octaves		= userData.octaves;
	PJS_roughness	= userData.roughness;

    if(runInParallel) {
    	curPosAndNor = initPos.mapPar(PJS_displace);
    }
    else {
	    curPosAndNor = initPos.map(PJS_displace);
    }
    if(golden_output === undefined)
        golden_output = curPosAndNor[5].pos[0]
    else if( golden_output !== curPosAndNor[5].pos[0] ) {
        print("Verfication failed");
    }
}
var start_time, elapsed_parallel = 0, elapsed_sequential = 0;
setup();

// Measure Parallel Execution
start_time = Date.now();
for(var no = 0; no < numIters; no++) {
    SimulatePJS(true);
}
elapsed_parallel += Date.now() - start_time;

// Measure Sequential Execution
start_time = Date.now();
for(no = 0; no < numIters; no++) {
    SimulatePJS(false);
}
elapsed_sequential += Date.now() - start_time;

print("Time taken for " + numIters + " iterations :: " + "[Parallel took " + elapsed_parallel + " ms],  " + "[Sequential took " + elapsed_sequential + " ms]");

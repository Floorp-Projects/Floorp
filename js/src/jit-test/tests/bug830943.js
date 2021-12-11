// |jit-test| error: Assertion failed: bad label: 2
try {
    this['Module'] = Module;
} catch (e) {
    this['Module'] = Module = {};
}
var ENVIRONMENT_IS_NODE = typeof process === 'object' && typeof require === 'function';
var ENVIRONMENT_IS_WEB = typeof window === 'object';
var ENVIRONMENT_IS_WORKER = typeof importScripts === 'function';
var ENVIRONMENT_IS_SHELL = !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_WORKER;
if (ENVIRONMENT_IS_SHELL) {
    Module['print'] = print;
    Module['arguments'] = [];
}
var Runtime = {
    alignMemory: function alignMemory(size, quantum) {},
}
    function SAFE_HEAP_CLEAR(dest) {
    }
    function SAFE_HEAP_STORE(dest, value, type, ignore) {
        setValue(dest, value, type, 1);
    }
    function SAFE_HEAP_LOAD(dest, type, unsigned, ignore) {
	try { } catch(e) {};
        var ret = getValue(dest, type, 1);
        return ret;
    };
    function SAFE_HEAP_LOAD1(dest, type) {
	return getValue(dest, type, 1);
    };
function abort(text) {
    Module.print(text + ':\n' + (new Error).stack);
    throw "Assertion: " + text;
}
function assert(condition, text) {
    if (!condition) {
        abort('Assertion failed: ' + text);
    }
}
function setValue(ptr, value, type, noSafe) {
    if (type.charAt(type.length - 1) === '*') type = 'i32'; // pointers are 32-bit
    if (noSafe) {
        switch (type) {
            case 'i32':
                HEAP32[((ptr) >> 2)] = value;
        }
    }
}
function getValue(ptr, type, noSafe) {
    if (type.charAt(type.length - 1) === '*') type = 'i32'; // pointers are 32-bit
    if (noSafe) {
        switch (type) {
            case 'i32':
                return HEAP32[((ptr) >> 2)];
        }
    }
}
var ALLOC_STATIC = 2; // Cannot be freed
function allocate(slab, types, allocator, ptr) {}
var TOTAL_MEMORY = Module['TOTAL_MEMORY'] || 16777216;
var buffer = new ArrayBuffer(TOTAL_MEMORY);
HEAP32 = new Int32Array(buffer);
STACK_ROOT = STACKTOP = Runtime.alignMemory(1);
function intArrayFromString(stringy, dontAddNull, length /* optional */ ) {}
function __ZN11btRigidBody14getMotionStateEv($this_0_20_val) {
}
function __ZN16btCollisionWorld23getCollisionObjectArrayEv($this) {}
function __ZN20btAlignedObjectArrayIP17btCollisionObjectEixEi($this_0_3_val, $n) {}
function _main($argc, $argv) {
    label = 2;
    while (1) switch (label) {
        case 2:
            var $31 = __Znwj(268);
            var $32 = $31;
            var $67 = __ZN17btCollisionObjectnwEj();
            var $68 = $67;
            __ZN23btDiscreteDynamicsWorld12addRigidBodyEP11btRigidBody($32, $68);
            var $99 = $31;
            var $104 = __ZN23btDiscreteDynamicsWorld14stepSimulationEfif($32, .01666666753590107, 10, .01666666753590107);
            var $106 = __ZNK16btCollisionWorld22getNumCollisionObjectsEv($99);
            var $108 = __ZN16btCollisionWorld23getCollisionObjectArrayEv($99);
            var $_idx6 = $108 + 12 | 0;
            var $j_049_in = $106;
            var $j_049 = $j_049_in - 1 | 0;
            var $_idx6_val = SAFE_HEAP_LOAD($_idx6, "%class.btCollisionObject**", 0, 0);
            var $109 = __ZN20btAlignedObjectArrayIP17btCollisionObjectEixEi($_idx6_val, $j_049);
            var $110 = SAFE_HEAP_LOAD($109, "%class.btCollisionObject*", 0, 0);
            var $111 = __ZN11btRigidBody6upcastEP17btCollisionObject($110);
            var $_idx9 = $111 + 472 | 0;
            var $_idx9_val = SAFE_HEAP_LOAD($_idx9, "%class.btMotionState*", 0, 0);
            var $114 = __ZN11btRigidBody14getMotionStateEv($_idx9_val);
            var $138 = $i_057 + 1 | 0;
            var $139 = ($138 | 0) < 135;
            if ($139) {
                var $i_057 = $138;
                break;
            }
            assert(0, "bad label: " + label);
    }
}
Module["_main"] = _main;
function __ZN17btCollisionObjectnwEj() {
    return __Z22btAlignedAllocInternalji(608);
}
function __ZNK16btCollisionWorld22getNumCollisionObjectsEv($this) {}
function __ZN11btRigidBody6upcastEP17btCollisionObject($colObj) {
    label = 2;
    while (1) switch (label) {
        case 2:
            var $_0;
            return $_0;
    }
}
function __ZNK9btVector33dotERKS_($this_0_0_0_val, $this_0_0_1_val, $this_0_0_2_val, $v_0_0_0_val, $v_0_0_1_val, $v_0_0_2_val) {
}
function __ZN20btAlignedObjectArrayIP11btRigidBodyEixEi($this_0_3_val, $n) {}
function __ZNK17btCollisionObject14getHitFractionEv($this_0_21_val) {}
function __ZN17btCollisionObject30getInterpolationWorldTransformEv($this) {}
function __ZNK17btCollisionObject30getInterpolationLinearVelocityEv($this) {}
function __ZNK17btCollisionObject31getInterpolationAngularVelocityEv($this) {}
function __ZN23btDiscreteDynamicsWorld28synchronizeSingleMotionStateEP11btRigidBody($this, $body) {
    assertEq($body, 16);
    var __stackBase__ = STACKTOP;
    while (1) switch (label) {
        case 2:
            var $interpolatedTransform = __stackBase__;
            var $4 = $body | 0;
            var $7 = __ZN17btCollisionObject30getInterpolationWorldTransformEv($4);
            var $8 = __ZNK17btCollisionObject30getInterpolationLinearVelocityEv($4);
            var $9 = __ZNK17btCollisionObject31getInterpolationAngularVelocityEv($4);
            var $10 = $this + 236 | 0;
            var $11 = SAFE_HEAP_LOAD($10, "float", 0, 0);
            var $_idx2 = $body + 240 | 0;
            var $_idx2_val = SAFE_HEAP_LOAD($_idx2, "float", 0, 0);
            var $12 = __ZNK17btCollisionObject14getHitFractionEv($_idx2_val);
            var $13 = $11 * $12;
            var $_idx3 = $8 | 0;
            var $_idx3_val = SAFE_HEAP_LOAD($_idx3, "float", 0, 0);
            var $_idx4 = $8 + 4 | 0;
            var $_idx4_val = SAFE_HEAP_LOAD($_idx4, "float", 0, 0);
            var $_idx5 = $8 + 8 | 0;
            var $_idx5_val = SAFE_HEAP_LOAD($_idx5, "float", 0, 0);
            __ZN15btTransformUtil18integrateTransformERK11btTransformRK9btVector3S5_fRS0_($7, $_idx3_val, $_idx4_val, $_idx5_val, $9, $13, $interpolatedTransform);
            return;
    }
}
function __ZN15btTransformUtil18integrateTransformERK11btTransformRK9btVector3S5_fRS0_($curTrans, $linvel_0_0_0_val, $linvel_0_0_1_val, $linvel_0_0_2_val, $angvel, $timeStep, $predictedTransform) {
    var __stackBase__ = STACKTOP;
    STACKTOP = STACKTOP + 132 | 0;
    while (1) {
	switch (label) {
        case 2:
            var $axis = __stackBase__ + 32;
            var $3 = __stackBase__ + 48;
            var $angvel_idx10 = $angvel | 0;
            var $angvel_idx10_val = SAFE_HEAP_LOAD($angvel_idx10, "float", 0, 0);
	    var $angvel_idx11 = $angvel + 4 | 0;
            var $angvel_idx11_val = SAFE_HEAP_LOAD($angvel_idx11, "float", 0, 0);
            var $angvel_idx12 = $angvel + 8 | 0;
            var $angvel_idx12_val = SAFE_HEAP_LOAD($angvel_idx12, "float", 0, 0);
            var $7 = __ZNK9btVector36lengthEv($angvel_idx10_val, $angvel_idx11_val, $angvel_idx12_val);
            var $8 = $7 * $timeStep;
            if ($8 > .7853981852531433) {} else {
                var $fAngle_0 = $7;
                label = 5;
            }
	    break;
        case 5:
            var $22 = $axis;
            var $23 = $3;
            SAFE_HEAP_STORE($22 + 12, SAFE_HEAP_LOAD1($23 + 12, "i32"), "i32", 0);
	    assertEq(SAFE_HEAP_LOAD1(0, "%class.btRigidBody*"), 16);
            label = 7;
            break;
        case 6:
            SAFE_HEAP_STORE($29 + 12, SAFE_HEAP_LOAD1($30 + 12, "i32"), "i32", 0);
        case 7:
            for (var i = __stackBase__; i < STACKTOP; i++) SAFE_HEAP_CLEAR(i);
            return;
	}
    }
}
function __ZN23btDiscreteDynamicsWorld12addRigidBodyEP11btRigidBody($this, $body) {
    SAFE_HEAP_STORE(STACKTOP, $body, "%class.btRigidBody*", 0);
    assertEq(SAFE_HEAP_LOAD(0, "%class.btRigidBody*", 0, 0), 16);
}
function __ZN23btDiscreteDynamicsWorld23synchronizeMotionStatesEv($this) {
    var $20 = SAFE_HEAP_LOAD(0, "%class.btRigidBody*", 0, 0);
    assertEq($20, 16);
    __ZN23btDiscreteDynamicsWorld28synchronizeSingleMotionStateEP11btRigidBody($this, $20);
}
function __ZN23btDiscreteDynamicsWorld14stepSimulationEfif($this, $timeStep, $maxSubSteps, $fixedTimeStep) {
    label = 2;
    while (1) switch (label) {
        case 2:
            var $numSimulationSubSteps_0;
            __ZN23btDiscreteDynamicsWorld23synchronizeMotionStatesEv($this);
            return $numSimulationSubSteps_0;
    }
}
function __ZNK9btVector37length2Ev($this_0_0_0_val, $this_0_0_1_val, $this_0_0_2_val) {
    return __ZNK9btVector33dotERKS_($this_0_0_0_val, $this_0_0_1_val, $this_0_0_2_val, $this_0_0_0_val, $this_0_0_1_val, $this_0_0_2_val);
}
function __Z6btSqrtf($y) {
    return Math.sqrt($y);
}
function __ZNK9btVector36lengthEv($this_0_0_0_val, $this_0_0_1_val, $this_0_0_2_val) {
    return __Z6btSqrtf(__ZNK9btVector37length2Ev($this_0_0_0_val, $this_0_0_1_val, $this_0_0_2_val));
}
function __ZL21btAlignedAllocDefaultji($size, $alignment) {
    while (1) switch (label) {
        case 2:
            var $1 = $size + 4 | 0;
            var $2 = $alignment - 1 | 0;
            var $3 = $1 + $2 | 0;
            var $4 = __ZL14btAllocDefaultj($3);
            var $7 = $4 + 4 | 0;
            var $8 = $7;
            var $9 = $alignment - $8 | 0;
            var $10 = $9 & $2;
            var $_sum = $10 + 4 | 0;
            var $11 = $4 + $_sum | 0;
            var $ret_0 = $11;
            return $ret_0;
    }
}
function __ZL14btAllocDefaultj($size) {
    return _malloc($size);
}
function __Z22btAlignedAllocInternalji($size) {
    return __ZL21btAlignedAllocDefaultji($size, 16);
}
function _malloc($bytes) {
    while (1) switch (label) {
        case 2:
            var $189 = SAFE_HEAP_LOAD(5244020, "%struct.malloc_chunk*", 0, 0);
            var $198 = $189 + 8 | 0;
            var $199 = $198;
            var $mem_0 = $199;
            return $mem_0;
    }
}
function __Znwj($size) {
    while (1) switch (label) {
        case 2:
            var $1 = ($size | 0) == 0;
            var $_size = $1 ? 1 : $size;
            var $3 = _malloc($_size);
            return $3;
    }
}
Module.callMain = function callMain(args) {
    var argc = args.length + 1;
    var argv = [allocate(intArrayFromString("/bin/this.program"), 'i8', ALLOC_STATIC)];
    return Module['_main'](argc, argv, 0);
}
function run(args) {
    args = args || Module['arguments'];
    function doRun() {
        if (Module['_main']) {
            ret = Module.callMain(args);
        }
    }
    if (Module['setStatus']) {} else {
        return doRun();
    }
}
run();

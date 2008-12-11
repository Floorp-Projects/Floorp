/*
 * Check that only JS_REQUIRES_STACK/JS_FORCES_STACK functions, and functions
 * that have called a JS_FORCES_STACK function, access cx->fp directly or
 * indirectly.
 */

require({ after_gcc_pass: 'cfg' });
include('gcc_util.js');
include('unstable/adts.js');
include('unstable/analysis.js');
include('unstable/lazy_types.js');
include('unstable/esp.js');

var Zero_NonZero = {};
include('unstable/zero_nonzero.js', Zero_NonZero);

// Tell MapFactory we don't need multimaps (a speed optimization).
MapFactory.use_injective = true;

/*
 * There are two regions in the program: RED and GREEN.  Functions and member
 * variables may be declared RED in the C++ source.  GREEN is the default.
 *
 * RED signals danger.  A GREEN part of a function must not call a RED function
 * or access a RED member.
 *
 * The body of a RED function is all red.  The body of a GREEN function is all
 * GREEN by default, but parts dominated by a call to a TURN_RED function are
 * red.  This way GREEN functions can safely access RED stuff by calling a
 * TURN_RED function as preparation.
 *
 * The analysis does not attempt to prove anything about the body of a TURN_RED
 * function.  (Both annotations are trusted; only unannotated code is checked
 * for errors.)
 */
const RED = 'JS_REQUIRES_STACK';
const TURN_RED = 'JS_FORCES_STACK';

function attrs(tree) {
  let a = DECL_P(tree) ? DECL_ATTRIBUTES(tree) : TYPE_ATTRIBUTES(TREE_TYPE(tree));
  return translate_attributes(a);
}

function hasUserAttribute(tree, attrname) {
  let attributes = attrs(tree);
  if (attributes) {
    for (let i = 0; i < attributes.length; i++) {
      let attr = attributes[i];
      if (attr.name == 'user' && attr.value.length == 1 && attr.value[0] == attrname)
        return true;
    }
  }
  return false;
}

/*
 * x is an expression or decl.  These functions assume that 
 */
function isRed(x) { return hasUserAttribute(x, RED); }
function isTurnRed(x) { return hasUserAttribute(x, TURN_RED); }

function process_tree(fndecl)
{
  if (!(isRed(fndecl) || isTurnRed(fndecl))) {
    // Ordinarily a user of ESP runs the analysis, then generates output based
    // on the results.  But in our case (a) we need sub-basic-block resolution,
    // which ESP doesn't keep; (b) it so happens that even though ESP can
    // iterate over blocks multiple times, in our case that won't cause
    // spurious output.  (It could cause us to the same error message each time
    // through--but that's easily avoided.)  Therefore we generate the output
    // while the ESP analysis is running.
    let a = new RedGreenCheck(fndecl, 0);
    if (a.hasRed)
      a.run();
  }
}

function RedGreenCheck(fndecl, trace) {
  //print("RedGreenCheck: " + fndecl.toCString());
  this._fndecl = fndecl;

  // Tell ESP that fndecl is a "property variable".  This makes ESP track it in
  // a flow-sensitive way.  The variable will be 1 in RED regions and "don't
  // know" in GREEN regions.  (We are technically lying to ESP about fndecl
  // being a variable--what we really want is a synthetic variable indicating
  // RED/GREEN state, but ESP operates on GCC decl nodes.)
  this._state_var_decl = fndecl;
  let state_var = new ESP.PropVarSpec(this._state_var_decl, true, undefined);

  // Call base class constructor.
  let cfg = function_decl_cfg(fndecl);
  ESP.Analysis.apply(this, [cfg, [state_var], Zero_NonZero.meet, trace]);
  this.join = Zero_NonZero.join;

  // Preprocess all instructions in the cfg to determine whether this analysis
  // is necessary and gather some information we'll use later.
  //
  // Each isn may include a function call, an assignment, and/or some reads.
  // Using walk_tree to walk the isns is a little crazy but robust.
  //
  this.hasRed = false;
  for (let bb in cfg_bb_iterator(cfg)) {
    for (let isn in bb_isn_iterator(bb)) {
      walk_tree(isn, function(t, stack) {
        switch (TREE_CODE(t)) {
          case FIELD_DECL:
            if (isRed(t)) {
              let varName = dehydra_convert(t).name;
              // location_of(t) is the location of the declaration.
              isn.redInfo = ["cannot access JS_REQUIRES_STACK variable " + varName,
                             location_of(stack[stack.length - 1])];
              this.hasRed = true;
            }
            break;
          case CALL_EXPR:
          {
            let callee = call_function_decl(t);
            if (callee) {
              if (isRed(callee)) {
                let calleeName = dehydra_convert(callee).name;
                isn.redInfo = ["cannot call JS_REQUIRES_STACK function " + calleeName,
                              location_of(t)];
                this.hasRed = true;
              } else if (isTurnRed(callee)) {
                isn.turnRed = true;
              }
            }
          }
          break;
        }
      });
    }
  }

  // Initialize mixin for infeasible-path elimination.
  this._zeroNonzero = new Zero_NonZero.Zero_NonZero();
}

RedGreenCheck.prototype = new ESP.Analysis;

RedGreenCheck.prototype.flowStateCond = function(isn, truth, state) {
  // forward event to mixin
  this._zeroNonzero.flowStateCond(isn, truth, state);
};

RedGreenCheck.prototype.flowState = function(isn, state) {
  // forward event to mixin
  //try { // The try/catch here is a workaround for some baffling bug in zero_nonzero.
    this._zeroNonzero.flowState(isn, state);
  //} catch (exc) {
  //  warning(exc, location_of(isn));
  //  warning("(Remove the workaround in jsstack.js and recompile to get a JS stack trace.)",
  //          location_of(isn));
  //}
  let green = (state.get(this._state_var_decl) != 1);
  let redInfo = isn.redInfo;
  if (green && redInfo) {
    error(redInfo[0], redInfo[1]);
    delete isn.redInfo;  // avoid duplicate messages about this instruction
  }

  // If we call a TURNS_RED function, it doesn't take effect until after the
  // whole isn finishes executing (the most conservative rule).
  if (isn.turnRed)
    state.assignValue(this._state_var_decl, 1, isn);
};


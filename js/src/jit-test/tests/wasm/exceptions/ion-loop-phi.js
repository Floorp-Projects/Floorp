// Finishing a loop will try to minimize phi nodes. We must properly replace
// phi nodes that escape a loop via catch block control flow patches.
wasmEvalText(`(module
  (func)
  (func (local i32)
    try
      loop
        call 0
        i32.const 0
        br_if 0
      end
    catch_all
    end
  )
)`);

// Same as above, but ensure that we check every enclosing try block for
// control flow patches, as delegate can tunnel outwards.
wasmEvalText(`(module
  (func)
  (func (local i32)
    try
      try
        loop
          call 0
          i32.const 0
          br_if 0
        end
      delegate 0
    catch_all
    end
  )
)`);

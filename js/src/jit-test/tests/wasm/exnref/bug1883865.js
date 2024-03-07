// Checks proper padding for nested tryNotes.

new WebAssembly.Module(wasmTextToBinary(`(module
  (func
      try_table $l3
        try_table $l4
          try_table $l5
          end
        end
      end
  )
)`));

new WebAssembly.Module(wasmTextToBinary(`(module
  (func
      try_table $l3
        try_table $l4
          try_table $l5
          end
          try_table $l5a
          end
        end
      end
  )
)`));

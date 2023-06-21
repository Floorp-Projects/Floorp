// |jit-test| skip-if: !wasmGcEnabled()

// Checking that we are correctly validating all subtyping rules.
// In this example, $b should be a subtype of $a, even if their field types
// will be loaded later.
wasmValidateText(`
(module
    (rec
        (type $a (sub (struct (field (ref $notParsedYet)))))
        (type $b (sub $a (struct (field (ref $notParsedYet2)))))

        (type $notParsedYet (sub (struct)))
        (type $notParsedYet2 (sub $notParsedYet (struct (field i32))))
    )
)`);

(progn
  (defparameter *trw*
    (generate-world
     "TTR"
     '((grammar test-to-rational-grammar :lalr-1 :start)
       
       (production :start () start-none)
       
       (define (to-rational (x finite-general-number)) rational
         (case x
           (:select (tag +zero32 +zero64 -zero32 -zero64) (return 0))
           (:narrow (union nonzero-finite-float32 nonzero-finite-float64 long u-long) (return (& value x))))))))
  
  (defparameter *trg* (world-grammar *trw* 'test-to-rational-grammar)))

(length (grammar-states *trg*))

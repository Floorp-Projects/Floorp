(progn
  (defparameter *sfw*
    (generate-world
     "SF"
     '((grammar standard-function-grammar :lalr-1 :start)
       
       (production :start () start-none)
       
       (deftuple x-long (value (integer-range (neg (expt 2 63)) (- (expt 2 63) 1))))
       (deftuple x-u-long (value (integer-range 0 (- (expt 2 64) 1))))
       (deftype x-general-number (union long u-long float32 float64))
       (deftype x-finite-general-number (union long u-long finite-float32 finite-float64))
       
       (define (x-digit-value (c character)) integer
         (cond
          ((set-in c (range-set-of-ranges character #\0 #\9))
           (return (- (character-to-code c) (character-to-code #\0))))
          ((set-in c (range-set-of-ranges character #\A #\Z))
           (return (+ (- (character-to-code c) (character-to-code #\A)) 10)))
          ((set-in c (range-set-of-ranges character #\a #\z))
           (return (+ (- (character-to-code c) (character-to-code #\a)) 10)))
          (nil (bottom))))
       
       (define (x-real-to-float64 (x rational)) float64
         (const s (range-set integer) (range-set-of integer (neg (expt 2 1024)) 0 (expt 2 1024)))
         (const a integer (integer-set-min s))
         (cond
          ((= a (expt 2 1024)) (return +infinity64))
          ((= a (neg (expt 2 1024))) (return -infinity64))
          ((/= a 0) (return (bottom)))
          ((< x 0 rational) (return -zero64))
          (nil (return +zero64))))
       
       (define (x-float32-to-float64 (x float32)) float64
         (case x
           (:select (tag +zero32) (return +zero64))
           (:select (tag -zero32) (return -zero64))
           (:select (tag +infinity32) (return +infinity64))
           (:select (tag -infinity32) (return -infinity64))
           (:select (tag nan32) (return nan64))
           (:narrow nonzero-finite-float32 (return (real-to-float64 (& value x))))))
       
       (define (x-truncate-finite-float64 (x finite-float64)) integer
         (rwhen (in x (tag +zero64 -zero64) :narrow-false)
           (return 0))
         (const r rational (& value x))
         (if (> r 0 rational)
           (return (floor r))
           (return (ceiling r))))
       
       #|(define (compare (x rational) (y rational)) order
         (cond
          ((< x y rational) (return less))
          ((= x y rational) (return equal))
          (nil (return greater))))|#
       )))
  
  (defparameter *sfg* (world-grammar *sfw* 'standard-function-grammar)))

#|
(values
 (depict-rtf-to-local-file
  "Test/StandardFunctionSemantics.rtf"
  "Standard Function Semantics"
  #'(lambda (rtf-stream)
      (depict-world-commands rtf-stream *sfw* :heading-offset 1)))
 (depict-html-to-local-file
  "Test/StandardFunctionSemantics.html"
  "Standard Function Semantics"
  t
  #'(lambda (html-stream)
      (depict-world-commands html-stream *sfw* :heading-offset 1))
  :external-link-base ""))
|#

(length (grammar-states *sfg*))

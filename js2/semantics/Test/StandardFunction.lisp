(progn
  (defparameter *sfw*
    (generate-world
     "SF"
     '((grammar standard-function-grammar :lalr-1 :start)
       
       (production :start () start-none)
       
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
          ((= a (expt 2 1024)) (return +infinity))
          ((= a (neg (expt 2 1024))) (return -infinity))
          ((/= a 0) (return 78.0))
          ((< x 0 rational) (return -zero))
          (nil (return +zero))))
       
       (define (x-truncate-finite-float64 (x finite-float64)) integer
         (rwhen (in x (tag +zero -zero) :narrow-false)
           (return 0))
         (if (> x 0 rational)
           (return (floor x))
           (return (ceiling x))))
       
       (define (compare (x rational) (y rational)) order
         (cond
          ((< x y rational) (return less))
          ((= x y rational) (return equal))
          (nil (return greater))))
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

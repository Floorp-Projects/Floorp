(progn
  (defparameter *sfw*
    (generate-world
     "SF"
     '((grammar standard-function-grammar :lalr-1 :start)
       
       (production :start () start-none)
       
       (define (x-digit-value (c character)) integer
         (if (character-set-member c (set-of-ranges character #\0 #\9))
           (- (character-to-code c) (character-to-code #\0))
           (if (character-set-member c (set-of-ranges character #\A #\Z))
             (+ (- (character-to-code c) (character-to-code #\A)) 10)
             (if (character-set-member c (set-of-ranges character #\a #\z))
               (+ (- (character-to-code c) (character-to-code #\a)) 10)
               (bottom integer)))))
       )))
  
  (defparameter *sfg* (world-grammar *sfw* 'standard-function-grammar)))

#|
(depict-rtf-to-local-file
 ";Test;StandardFunctionSemantics.rtf"
 "Standard Function Semantics"
 #'(lambda (rtf-stream)
     (depict-world-commands rtf-stream *sfw*)))

(depict-html-to-local-file
 ";Test;StandardFunctionSemantics.html"
 "Standard Function Semantics"
 t
 #'(lambda (html-stream)
     (depict-world-commands html-stream *sfw*))
 :external-link-base "")
|#

(length (grammar-states *sfg*))

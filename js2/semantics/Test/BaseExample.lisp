(progn
  (defparameter *bew*
    (generate-world
     "BE"
     '((lexer base-example-lexer
              :lalr-1
              :numeral
              ((:digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9) ((value $digit-value))))
              (($digit-value integer digit-value digit-char-36)))
       
       (deftag syntax-error)
       (deftype semantic-exception (tag syntax-error))
       
       (%charclass :digit)
       
       (rule :digits ((decimal-value integer)
                      (base-value (-> (integer) integer)))
         (production :digits (:digit) digits-first
           (decimal-value (value :digit))
           ((base-value base)
            (const d integer (value :digit))
            (if (< d base)
              (return d)
              (throw syntax-error))))
         (production :digits (:digits :digit) digits-rest
           (decimal-value (+ (* 10 (decimal-value :digits)) (value :digit)))
           ((base-value base)
            (const d integer (value :digit))
            (if (< d base)
              (return (+ (* base ((base-value :digits) base)) d))
              (throw syntax-error)))))
       
       (rule :numeral ((value integer))
         (production :numeral (:digits) numeral-digits
           (value (decimal-value :digits)))
         (production :numeral (:digits #\# :digits) numeral-digits-and-base
           (value
            (begin
             (const base integer (decimal-value :digits 2))
             (if (and (>= base 2) (<= base 10))
               (return ((base-value :digits 1) base))
               (throw syntax-error))))))
       (%print-actions)
       )))
  
  (defparameter *bel* (world-lexer *bew* 'base-example-lexer))
  (defparameter *beg* (lexer-grammar *bel*)))

#|
(values
  (depict-rtf-to-local-file
   "Test/BaseExampleSemantics.rtf"
   "Base Example Semantics"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *bew* :heading-offset 1)))
  (depict-html-to-local-file
   "Test/BaseExampleSemantics.html"
   "Base Example Semantics"
   t
   #'(lambda (html-stream)
       (depict-world-commands html-stream *bew* :heading-offset 1))
   :external-link-base ""))


(lexer-pparse *bel* "37")
(lexer-pparse *bel* "33#4")
(lexer-pparse *bel* "30#2")

|#

(length (grammar-states *beg*))

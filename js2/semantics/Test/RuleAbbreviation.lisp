(progn
  (defparameter *raw*
    (generate-world
     "RA"
     '((grammar rule-abbreviation-grammar :lalr-1 :expression)
       
       (deftag syntax-error)
       (deftype context integer)
       (deftype environment integer)
       (deftype semantic-exception (tag syntax-error))
       
       (rule :subexpression ((validate (-> (context environment) void)))
         (production :subexpression (keyword) subexpression-keyword
           ((validate (cxt :unused) (env :unused)))))
       
       (rule :expression ((validate (-> (context environment) void)) (validate2 (-> (context environment) void)))
         (production :expression (:subexpression) expression-unary
           ((validate cxt env) :forward)
           ((validate2 cxt env)
            ((validate :subexpression) cxt env)))
         (production :expression (:expression * :subexpression) expression-multiply
           ((validate cxt env) :forward)
           ((validate2 cxt env)
            ((validate :expression) cxt env)
            ((validate :subexpression) cxt env)))
         (production :expression (:subexpression + :subexpression) expression-add
           ((validate cxt env) :forward)
           ((validate2 cxt env)
            ((validate :subexpression 1) cxt env)
            ((validate :subexpression 2) cxt env)))
         (production :expression (this) expression-this
           ((validate cxt env) :forward)
           ((validate2 (cxt :unused) (env :unused)))))
       (%print-actions ("Validation" validate) ("Evaluation" eval))
       )))
  
  (defparameter *rag* (world-grammar *raw* 'rule-abbreviation-grammar)))

#|
(values
  (depict-rtf-to-local-file
   "Test/RuleAbbreviationSemantics.rtf"
   "Rule Abbreviation Semantics"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *raw* :heading-offset 1)))
  (depict-html-to-local-file
   "Test/RuleAbbreviationSemantics.html"
   "Rule Abbreviation Semantics"
   t
   #'(lambda (html-stream)
       (depict-world-commands html-stream *raw* :heading-offset 1))
   :external-link-base ""))
|#

(length (grammar-states *rag*))

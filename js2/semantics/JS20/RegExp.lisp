;;;
;;; JavaScript 2.0 regular expression parser
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(progn
  (defparameter *rw*
    (generate-world
     "R"
     '((lexer regexp-lexer
              :lr-1
              :regular-expression-pattern
              ((:unicode-character (% every (:text "Any Unicode character")) () t)
               (:unicode-alphanumeric
                (% alphanumeric (:text "Any Unicode alphabetic or decimal digit character (includes ASCII "
                                       (:character-literal #\0) :nbhy (:character-literal #\9) ", "
                                       (:character-literal #\A) :nbhy (:character-literal #\Z) ", and "
                                       (:character-literal #\a) :nbhy (:character-literal #\z) ")"))
                () t)
               (:line-terminator (#?000A #?000D #?2028 #?2029) () t)
               (:decimal-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                               (($default-action $default-action)
                                (decimal-value $digit-value)))
               (:non-zero-digit (#\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                ((decimal-value $digit-value)))
               (:hex-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9 #\A #\B #\C #\D #\E #\F #\a #\b #\c #\d #\e #\f)
                           ((hex-value $digit-value)))
               (:control-letter (++ (#\A #\B #\C #\D #\E #\F #\G #\H #\I #\J #\K #\L #\M #\N #\O #\P #\Q #\R #\S #\T #\U #\V #\W #\X #\Y #\Z)
                                    (#\a #\b #\c #\d #\e #\f #\g #\h #\i #\j #\k #\l #\m #\n #\o #\p #\q #\r #\s #\t #\u #\v #\w #\x #\y #\z))
                                (($default-action $default-action)))
               (:pattern-character (- :unicode-character (#\^ #\$ #\\ #\. #\* #\+ #\? #\( #\) #\[ #\] #\{ #\} #\|))
                                   (($default-action $default-action)))
               ((:class-character dash) (- :unicode-character (#\\ #\]))
                (($default-action $default-action)))
               ((:class-character no-dash) (- (:class-character dash) (#\-))
                (($default-action $default-action)))
               (:identity-escape (- :unicode-character (+ (#\_) :unicode-alphanumeric))
                                 (($default-action $default-action))))
              (($default-action character nil identity)
               ($digit-value integer digit-value digit-char-36)))
       
       (deftag syntax-error)
       (deftype semantic-exception (tag syntax-error))
       
       (%heading 1 "Unicode Character Classes")
       (%charclass :unicode-character)
       (%charclass :unicode-alphanumeric)
       (%charclass :line-terminator)
       
       (define line-terminators (range-set character) (range-set-of character #?000A #?000D #?2028 #?2029))
       (define re-whitespaces (range-set character) (range-set-of character #?000C #?000A #?000D #?0009 #?000B #\space))
       (define re-digits (range-set character) (range-set-of-ranges character #\0 #\9))
       (define re-word-characters (range-set character) (range-set-of-ranges character #\0 #\9 #\A #\Z #\a #\z #\_ nil))
       (%print-actions)
       
       
       (%heading 1 "Regular Expression Definitions")
       (deftuple r-e-input (str string) (ignore-case boolean) (multiline boolean) (span boolean))
       (%text :semantics
         "Field " (:label r-e-input str) " is the input string. "
         (:label r-e-input ignore-case) ", "
         (:label r-e-input multiline) ", and "
         (:label r-e-input span) " are the corresponding regular expression flags.")
       
       (deftag undefined)
       (deftype capture (union string (tag undefined)))
       
       (deftuple r-e-match (end-index integer) (captures (vector capture)))
       (deftag failure)
       (deftype r-e-result (union r-e-match (tag failure)))
       (%text :semantics
         "A " (:type r-e-match) " holds an intermediate state during the pattern-matching process. "
         (:label r-e-match end-index)
         " is the index of the next input character to be matched by the next component in a regular expression pattern. "
         "If we are at the end of the pattern, " (:label r-e-match end-index)
         " is one plus the index of the last matched input character. "
         (:label r-e-match captures)
         " is a zero-based array of the strings captured so far by capturing parentheses.")
       
       (deftype continuation (-> (r-e-match) r-e-result))
       (%text :semantics
         "A " (:type continuation)
         " is a function that attempts to match the remaining portion of the pattern against the input string, "
         "starting at the intermediate state given by its " (:type r-e-match) " argument. "
         "If a match is possible, it returns a " (:type r-e-match)
         " result that contains the final state; if no match is possible, it returns a " (:tag failure) " result.")
       
       (deftype matcher (-> (r-e-input r-e-match continuation) r-e-result))
       (%text :semantics
         "A " (:type matcher)
         " is a function that attempts to match a middle portion of the pattern against the input string, "
         "starting at the intermediate state given by its " (:type r-e-match) " argument. "
         "Since the remainder of the pattern heavily influences whether (and how) a middle portion will match, we "
         "must pass in a " (:type continuation) " function that checks whether the rest of the pattern matched. "
         "If the continuation returns " (:tag failure) ", the matcher function may call it repeatedly, "
         "trying various alternatives at pattern choice points.")
       (%text :semantics
         "The " (:type r-e-input) " parameter contains the input string and is merely passed down to subroutines.")
       
       (%text :semantics
         "A " (:type (-> (integer) matcher))
         " is a function executed at the time the regular expression is compiled that returns a " (:type matcher) " for a part "
         "of the pattern. The " (:type integer) " parameter contains the number of capturing left parentheses seen so far in the "
         "pattern and is used to assign static, consecutive numbers to capturing parentheses.")
       
       (define (character-set-matcher (acceptance-set (range-set character)) (invert boolean)) matcher   ;*********ignore case?
         (function (m (t r-e-input) (x r-e-match) (c continuation)) r-e-result
           (const i integer (& end-index x))
           (const s string (& str t))
           (cond
            ((= i (length s)) (return failure))
            ((xor (set-in (nth s i) acceptance-set) invert)
             (return (c (new r-e-match (+ i 1) (& captures x)))))
            (nil (return failure))))
         (return m))
       (%text :semantics
         (:global character-set-matcher) " returns a " (:type matcher)
         " that matches a single input string character. If "
         (:local invert) " is " (:tag false) ", the match succeeds if the character is a member of the "
         (:local acceptance-set) " set of characters (possibly ignoring case). If "
         (:local invert) " is " (:tag true) ", the match succeeds if the character is not a member of the "
         (:local acceptance-set) " set of characters (possibly ignoring case).")
       
       (define (character-matcher (ch character)) matcher
         (return (character-set-matcher (range-set-of character ch) false)))
       (%text :semantics
         (:global character-matcher) " returns a " (:type matcher)
         " that matches a single input string character. The match succeeds if the character is the same as "
         (:local ch) " (possibly ignoring case).")
       
       (%print-actions)
       
       
       (%heading 1 "Regular Expression Patterns")
       
       (rule :regular-expression-pattern ((execute (-> (r-e-input integer) r-e-result)))
         (production :regular-expression-pattern (:disjunction) regular-expression-pattern-disjunction
           (execute
            (begin
             (const m1 matcher ((gen-matcher :disjunction) 0))
             (function (e (t r-e-input) (index integer)) r-e-result
               (const x r-e-match (new r-e-match index (fill-capture (count-parens :disjunction))))
               (return (m1 t x success-continuation)))
             (return e)))))
       
       (%print-actions)
       (define (success-continuation (x r-e-match)) r-e-result
         (return x))
       (define (fill-capture (i integer)) (vector capture)
         (if (= i 0)
           (return (vector-of capture))
           (return (append (fill-capture (- i 1)) (vector-of capture undefined)))))
       
       
       (%heading 2 "Disjunctions")
       
       (rule :disjunction ((gen-matcher (-> (integer) matcher)) (count-parens integer))
         (production :disjunction (:alternative) disjunction-one
           ((gen-matcher paren-index) (return ((gen-matcher :alternative) paren-index)))
           (count-parens (count-parens :alternative)))
         (production :disjunction (:alternative #\| :disjunction) disjunction-more
           ((gen-matcher paren-index)
            (const m1 matcher ((gen-matcher :alternative) paren-index))
            (const m2 matcher ((gen-matcher :disjunction) (+ paren-index (count-parens :alternative))))
            (function (m3 (t r-e-input) (x r-e-match) (c continuation)) r-e-result
              (const y r-e-result (m1 t x c))
              (case y
                (:select r-e-match (return y))
                (:select (tag failure) (return (m2 t x c)))))
            (return m3))
           (count-parens (+ (count-parens :alternative) (count-parens :disjunction)))))
       
       (%print-actions)
       
       
       (%heading 2 "Alternatives")
       
       (rule :alternative ((gen-matcher (-> (integer) matcher)) (count-parens integer))
         (production :alternative () alternative-none
           ((gen-matcher (paren-index :unused))
            (function (m (t r-e-input :unused) (x r-e-match) (c continuation)) r-e-result
              (return (c x)))
            (return m))
           (count-parens 0))
         (production :alternative (:alternative :term) alternative-some
           ((gen-matcher paren-index)
            (const m1 matcher ((gen-matcher :alternative) paren-index))
            (const m2 matcher ((gen-matcher :term) (+ paren-index (count-parens :alternative))))
            (function (m3 (t r-e-input) (x r-e-match) (c continuation)) r-e-result
              (function (d (y r-e-match)) r-e-result
                (return (m2 t y c)))
              (return (m1 t x d)))
            (return m3))
           (count-parens (+ (count-parens :alternative) (count-parens :term)))))
       
       (%print-actions)
       
       
       (%heading 2 "Terms")
       
       (rule :term ((gen-matcher (-> (integer) matcher)) (count-parens integer))
         (production :term (:assertion) term-assertion
           ((gen-matcher (paren-index :unused))
            (function (m (t r-e-input) (x r-e-match) (c continuation)) r-e-result
              (if ((test-assertion :assertion) t x)
                (return (c x))
                (return failure)))
            (return m))
           (count-parens 0))
         (production :term (:atom) term-atom
           ((gen-matcher paren-index) (return ((gen-matcher :atom) paren-index)))
           (count-parens (count-parens :atom)))
         (production :term (:atom :quantifier) term-quantified-atom
           ((gen-matcher paren-index)
            (const m matcher ((gen-matcher :atom) paren-index))
            (const min integer (minimum :quantifier))
            (const max limit (maximum :quantifier))
            (const greedy boolean (greedy :quantifier))
            (when (not-in max (tag +infinity) :narrow-true)
              (rwhen (< max min)
                (throw syntax-error)))
            (return (repeat-matcher m min max greedy paren-index (count-parens :atom))))
           (count-parens (count-parens :atom))))
       
       (%print-actions)
       
       
       (rule :quantifier ((minimum integer) (maximum limit) (greedy boolean))
         (production :quantifier (:quantifier-prefix) quantifier-eager
           (minimum (minimum :quantifier-prefix))
           (maximum (maximum :quantifier-prefix))
           (greedy true))
         (production :quantifier (:quantifier-prefix #\?) quantifier-greedy
           (minimum (minimum :quantifier-prefix))
           (maximum (maximum :quantifier-prefix))
           (greedy false)))
       
       (rule :quantifier-prefix ((minimum integer) (maximum limit))
         (production :quantifier-prefix (#\*) quantifier-prefix-zero-or-more
           (minimum 0)
           (maximum +infinity))
         (production :quantifier-prefix (#\+) quantifier-prefix-one-or-more
           (minimum 1)
           (maximum +infinity))
         (production :quantifier-prefix (#\?) quantifier-prefix-zero-or-one
           (minimum 0)
           (maximum 1))
         (production :quantifier-prefix (#\{ :decimal-digits #\}) quantifier-prefix-repeat
           (minimum (integer-value :decimal-digits))
           (maximum (integer-value :decimal-digits)))
         (production :quantifier-prefix (#\{ :decimal-digits #\, #\}) quantifier-prefix-repeat-or-more
           (minimum (integer-value :decimal-digits))
           (maximum +infinity))
         (production :quantifier-prefix (#\{ :decimal-digits #\, :decimal-digits #\}) quantifier-prefix-repeat-range
           (minimum (integer-value :decimal-digits 1))
           (maximum (integer-value :decimal-digits 2))))
       
       (rule :decimal-digits ((integer-value integer))
         (production :decimal-digits (:decimal-digit) decimal-digits-first
           (integer-value (decimal-value :decimal-digit)))
         (production :decimal-digits (:decimal-digits :decimal-digit) decimal-digits-rest
           (integer-value (+ (* 10 (integer-value :decimal-digits)) (decimal-value :decimal-digit)))))
       (%charclass :decimal-digit)
       
       
       (deftype limit (union integer (tag +infinity)))
       
       (define (reset-parens (x r-e-match) (p integer) (n-parens integer)) r-e-match
         (var captures (vector capture) (& captures x))
         (var i integer p)
         (while (< i (+ p n-parens))
           (<- captures (set-nth captures i undefined))
           (<- i (+ i 1)))
         (return (new r-e-match (& end-index x) captures)))
       
       (define (repeat-matcher (body matcher) (min integer) (max limit) (greedy boolean) (paren-index integer) (n-body-parens integer)) matcher
         (function (m (t r-e-input) (x r-e-match) (c continuation)) r-e-result
           (rwhen (= max 0 limit)
             (return (c x)))
           (function (d (y r-e-match)) r-e-result
             (rwhen (and (= min 0) (= (& end-index y) (& end-index x)))
               (return failure))
             (var new-min integer min)
             (when (/= min 0)
               (<- new-min (- min 1)))
             (var new-max limit max)
             (when (not-in max (tag +infinity) :narrow-true)
               (<- new-max (- max 1)))
             (const m2 matcher (repeat-matcher body new-min new-max greedy paren-index n-body-parens))
             (return (m2 t y c)))
           (const xr r-e-match (reset-parens x paren-index n-body-parens))
           (cond
            ((/= min 0) (return (body t xr d)))
            (greedy
             (const z r-e-result (body t xr d))
             (case z
               (:select r-e-match (return z))
               (:select (tag failure) (return (c x)))))
            (nil
             (const z r-e-result (c x))
             (case z
               (:select r-e-match (return z))
               (:select (tag failure) (return (body t xr d)))))))
         (return m))
       
       (%print-actions)
       
       
       (%heading 2 "Assertions")
       
       (rule :assertion ((test-assertion (-> (r-e-input r-e-match) boolean)))
         (production :assertion (#\^) assertion-beginning
           ((test-assertion t x)
            (return (or (= (& end-index x) 0)
                        (and (& multiline t)
                             (set-in (nth (& str t) (- (& end-index x) 1)) line-terminators))))))
         (production :assertion (#\$) assertion-end
           ((test-assertion t x)
            (return (or (= (& end-index x) (length (& str t)))
                        (and (& multiline t)
                             (set-in (nth (& str t) (& end-index x)) line-terminators))))))
         (production :assertion (#\\ #\b) assertion-word-boundary
           ((test-assertion t x)
            (return (at-word-boundary (& end-index x) (& str t)))))
         (production :assertion (#\\ #\B) assertion-non-word-boundary
           ((test-assertion t x)
            (return (not (at-word-boundary (& end-index x) (& str t)))))))
       
       (%print-actions)
       
       (define (at-word-boundary (i integer) (s string)) boolean
         (return (xor (in-word (- i 1) s) (in-word i s))))
       
       (define (in-word (i integer) (s string)) boolean
         (if (or (= i -1) (= i (length s)))
           (return false)
           (return (set-in (nth s i) re-word-characters))))
       
       
       (%heading 1 "Atoms")
       
       (rule :atom ((gen-matcher (-> (integer) matcher)) (count-parens integer))
         (production :atom (:pattern-character) atom-pattern-character
           ((gen-matcher (paren-index :unused))
            (return (character-matcher ($default-action :pattern-character))))
           (count-parens 0))
         (production :atom (#\.) atom-dot
           ((gen-matcher (paren-index :unused))
            (function (m1 (t r-e-input) (x r-e-match) (c continuation)) r-e-result
              (const a (range-set character) (if (& span t) (range-set-of character) line-terminators))
              (const m2 matcher (character-set-matcher a true))
              (return (m2 t x c)))
            (return m1))
           (count-parens 0))
         (production :atom (:null-escape) atom-null-escape
           ((gen-matcher (paren-index :unused))
            (function (m (t r-e-input :unused) (x r-e-match) (c continuation)) r-e-result
              (return (c x)))
            (return m))
           (count-parens 0))
         (production :atom (#\\ :atom-escape) atom-atom-escape
           ((gen-matcher paren-index) (return ((gen-matcher :atom-escape) paren-index)))
           (count-parens 0))
         (production :atom (:character-class) atom-character-class
           ((gen-matcher (paren-index :unused))
            (const a (range-set character) (acceptance-set :character-class))
            (return (character-set-matcher a (invert :character-class))))
           (count-parens 0))
         (production :atom (#\( :disjunction #\)) atom-parentheses
           ((gen-matcher paren-index)
            (const m1 matcher ((gen-matcher :disjunction) (+ paren-index 1)))
            (function (m2 (t r-e-input) (x r-e-match) (c continuation)) r-e-result
              (function (d (y r-e-match)) r-e-result
                (const ref capture (subseq (& str t) (& end-index x) (- (& end-index y) 1)))
                (const updated-captures (vector capture)
                  (set-nth (& captures y) paren-index ref))
                (return (c (new r-e-match (& end-index y) updated-captures))))
              (return (m1 t x d)))
            (return m2))
           (count-parens (+ (count-parens :disjunction) 1)))
         (production :atom (#\( #\? #\: :disjunction #\)) atom-non-capturing-parentheses
           ((gen-matcher paren-index) (return ((gen-matcher :disjunction) paren-index)))
           (count-parens (count-parens :disjunction)))
         (production :atom (#\( #\? #\= :disjunction #\)) atom-positive-lookahead
           ((gen-matcher paren-index)
            (const m1 matcher ((gen-matcher :disjunction) paren-index))
            (function (m2 (t r-e-input) (x r-e-match) (c continuation)) r-e-result
              ;(function (d (y r-e-match)) r-e-result
              ;  (return (c (new r-e-match (& end-index x) (& captures y)))))
              ;(return (m1 t x d)))))
              (const y r-e-result (m1 t x success-continuation))
              (case y
                (:narrow r-e-match (return (c (new r-e-match (& end-index x) (& captures y)))))
                (:select (tag failure) (return failure))))
            (return m2))
           (count-parens (count-parens :disjunction)))
         (production :atom (#\( #\? #\! :disjunction #\)) atom-negative-lookahead
           ((gen-matcher paren-index)
            (const m1 matcher ((gen-matcher :disjunction) paren-index))
            (function (m2 (t r-e-input) (x r-e-match) (c continuation)) r-e-result
              (case (m1 t x success-continuation)
                (:select r-e-match (return failure))
                (:select (tag failure) (return (c x)))))
            (return m2))
           (count-parens (count-parens :disjunction))))
       
       (%charclass :pattern-character)
       (%print-actions)
       
       
       (%heading 1 "Escapes")
       
       (production :null-escape (#\\ #\_) null-escape-underscore)
       
       (rule :atom-escape ((gen-matcher (-> (integer) matcher)))
         (production :atom-escape (:decimal-escape) atom-escape-decimal
           ((gen-matcher paren-index)
            (const n integer (escape-value :decimal-escape))
            (cond
             ((= n 0) (return (character-matcher #?0000)))
             ((> n paren-index) (throw syntax-error))
             (nil (return (backreference-matcher n))))))
         (production :atom-escape (:character-escape) atom-escape-character
           ((gen-matcher (paren-index :unused))
            (return (character-matcher (character-value :character-escape)))))
         (production :atom-escape (:character-class-escape) atom-escape-character-class
           ((gen-matcher (paren-index :unused))
            (return (character-set-matcher (acceptance-set :character-class-escape) false)))))
       (%print-actions)
       
       (define (backreference-matcher (n integer)) matcher
         (function (m (t r-e-input) (x r-e-match) (c continuation)) r-e-result
           (const ref capture (nth-backreference x n))
           (case ref
             (:narrow string
               (const i integer (& end-index x))
               (const s string (& str t))
               (const j integer (+ i (length ref)))
               (if (and (<= j (length s))
                        (= (subseq s i (- j 1)) ref string))   ;*********ignore case?
                 (return (c (new r-e-match j (& captures x))))
                 (return failure)))
             (:select (tag undefined) (return (c x)))))
         (return m))
       
       (define (nth-backreference (x r-e-match) (n integer)) capture
         (return (nth (& captures x) (- n 1))))
       
       
       (rule :character-escape ((character-value character))
         (production :character-escape (:control-escape) character-escape-control
           (character-value (character-value :control-escape)))
         (production :character-escape (#\c :control-letter) character-escape-control-letter
           (character-value (code-to-character (bitwise-and (character-to-code ($default-action :control-letter)) 31))))
         (production :character-escape (:hex-escape) character-escape-hex
           (character-value (character-value :hex-escape)))
         (production :character-escape (:identity-escape) character-escape-identity
           (character-value ($default-action :identity-escape))))
       
       (%charclass :control-letter)
       (%charclass :identity-escape)
       
       (rule :control-escape ((character-value character))
         (production :control-escape (#\f) control-escape-form-feed (character-value #?000C))
         (production :control-escape (#\n) control-escape-new-line (character-value #?000A))
         (production :control-escape (#\r) control-escape-return (character-value #?000D))
         (production :control-escape (#\t) control-escape-tab (character-value #?0009))
         (production :control-escape (#\v) control-escape-vertical-tab (character-value #?000B)))
       (%print-actions)
       
       
       (%heading 2 "Decimal Escapes")
       
       (rule :decimal-escape ((escape-value integer))
         (production :decimal-escape (:decimal-integer-literal (:- :decimal-digit)) decimal-escape-integer
           (escape-value (integer-value :decimal-integer-literal))))
       
       (rule :decimal-integer-literal ((integer-value integer))
         (production :decimal-integer-literal (#\0) decimal-integer-literal-0
           (integer-value 0))
         (production :decimal-integer-literal (:non-zero-decimal-digits) decimal-integer-literal-nonzero
           (integer-value (integer-value :non-zero-decimal-digits))))
       
       (rule :non-zero-decimal-digits ((integer-value integer))
         (production :non-zero-decimal-digits (:non-zero-digit) non-zero-decimal-digits-first
           (integer-value (decimal-value :non-zero-digit)))
         (production :non-zero-decimal-digits (:non-zero-decimal-digits :decimal-digit) non-zero-decimal-digits-rest
           (integer-value (+ (* 10 (integer-value :non-zero-decimal-digits)) (decimal-value :decimal-digit)))))
       
       (%charclass :non-zero-digit)
       (%print-actions)
       
       
       (%heading 2 "Hexadecimal Escapes")
       
       (rule :hex-escape ((character-value character))
         (production :hex-escape (#\x :hex-digit :hex-digit) hex-escape-2
           (character-value (code-to-character (+ (* 16 (hex-value :hex-digit 1))
                                                  (hex-value :hex-digit 2)))))
         (production :hex-escape (#\u :hex-digit :hex-digit :hex-digit :hex-digit) hex-escape-4
           (character-value (code-to-character (+ (+ (+ (* 4096 (hex-value :hex-digit 1))
                                                        (* 256 (hex-value :hex-digit 2)))
                                                     (* 16 (hex-value :hex-digit 3)))
                                                  (hex-value :hex-digit 4))))))
       (%charclass :hex-digit)
       (%print-actions)
       
       
       (%heading 2 "Character Class Escapes")
       
       (rule :character-class-escape ((acceptance-set (range-set character)))
         (production :character-class-escape (#\s) character-class-escape-whitespace
           (acceptance-set re-whitespaces))
         (production :character-class-escape (#\S) character-class-escape-non-whitespace
           (acceptance-set (set- (range-set-of-ranges character #?0000 #?FFFF) re-whitespaces)))
         (production :character-class-escape (#\d) character-class-escape-digit
           (acceptance-set re-digits))
         (production :character-class-escape (#\D) character-class-escape-non-digit
           (acceptance-set (set- (range-set-of-ranges character #?0000 #?FFFF) re-digits)))
         (production :character-class-escape (#\w) character-class-escape-word
           (acceptance-set re-word-characters))
         (production :character-class-escape (#\W) character-class-escape-non-word
           (acceptance-set (set- (range-set-of-ranges character #?0000 #?FFFF) re-word-characters))))
       (%print-actions)
       
       
       (%heading 1 "User-Specified Character Classes")
       
       (rule :character-class ((acceptance-set (range-set character)) (invert boolean))
         (production :character-class (#\[ (:- #\^) :class-ranges #\]) character-class-positive
           (acceptance-set (acceptance-set :class-ranges))
           (invert false))
         (production :character-class (#\[ #\^ :class-ranges #\]) character-class-negative
           (acceptance-set (acceptance-set :class-ranges))
           (invert true)))
       
       (rule :class-ranges ((acceptance-set (range-set character)))
         (production :class-ranges () class-ranges-none
           (acceptance-set (range-set-of character)))
         (production :class-ranges ((:nonempty-class-ranges dash)) class-ranges-some
           (acceptance-set (acceptance-set :nonempty-class-ranges))))
       
       (grammar-argument :delta dash no-dash)
       
       (rule (:nonempty-class-ranges :delta) ((acceptance-set (range-set character)))
         (production (:nonempty-class-ranges :delta) ((:class-atom dash)) nonempty-class-ranges-final
           (acceptance-set (acceptance-set :class-atom)))
         (production (:nonempty-class-ranges :delta) ((:class-atom :delta) (:nonempty-class-ranges no-dash)) nonempty-class-ranges-non-final
           (acceptance-set
            (set+ (acceptance-set :class-atom)
                  (acceptance-set :nonempty-class-ranges))))
         (production (:nonempty-class-ranges :delta) ((:class-atom :delta) #\- (:class-atom dash) :class-ranges) nonempty-class-ranges-range
           (acceptance-set
            (set+ (character-range (acceptance-set :class-atom 1) (acceptance-set :class-atom 2))
                  (acceptance-set :class-ranges))))
         (production (:nonempty-class-ranges :delta) (:null-escape :class-ranges) nonempty-class-ranges-null-escape
           (acceptance-set (acceptance-set :class-ranges))))
       (%print-actions)
       
       (define (character-range (low (range-set character)) (high (range-set character))) (range-set character)
         (rwhen (or (/= (length low) 1) (/= (length high) 1))
           (throw syntax-error))
         (const l character (character-set-min low))
         (const h character (character-set-min high))
         (if (<= l h character)
           (return (range-set-of-ranges character l h))
           (throw syntax-error)))
       
       
       (%heading 2 "Character Class Range Atoms")
       
       (rule (:class-atom :delta) ((acceptance-set (range-set character)))
         (production (:class-atom :delta) ((:class-character :delta)) class-atom-character
           (acceptance-set (range-set-of character ($default-action :class-character))))
         (production (:class-atom :delta) (#\\ :class-escape) class-atom-escape
           (acceptance-set (acceptance-set :class-escape))))
       
       (%charclass (:class-character dash))
       (%charclass (:class-character no-dash))
       
       (rule :class-escape ((acceptance-set (range-set character)))
         (production :class-escape (:decimal-escape) class-escape-decimal
           (acceptance-set
            (begin
             (if (= (escape-value :decimal-escape) 0)
               (return (range-set-of character #?0000))
               (throw syntax-error)))))
         (production :class-escape (#\b) class-escape-backspace
           (acceptance-set (range-set-of character #?0008)))
         (production :class-escape (:character-escape) class-escape-character-escape
           (acceptance-set (range-set-of character (character-value :character-escape))))
         (production :class-escape (:character-class-escape) class-escape-character-class-escape
           (acceptance-set (acceptance-set :character-class-escape))))
       (%print-actions)
       )))
  
  (defparameter *rl* (world-lexer *rw* 'regexp-lexer))
  (defparameter *rg* (lexer-grammar *rl*)))


(eval-when (:load-toplevel :execute)
  (defun run-regexp (regexp input &key ignore-case multiline span)
    (let ((execute (first (lexer-parse *rl* regexp))))
      (dotimes (i (length input) :failure)
        (let ((result (funcall execute (list 'r:r-e-input input ignore-case multiline span) i)))
          (unless (eq result :failure)
            (assert-true (eq (first result) 'r:r-e-match))
            (return (list* i (subseq input i (second result)) (cddr result)))))))))

(defun dump-regexp ()
  (values
   (depict-rtf-to-local-file
    "JS20/RegExpGrammar.rtf"
    "Regular Expression Grammar"
    #'(lambda (rtf-stream)
        (depict-world-commands rtf-stream *rw* :heading-offset 1 :visible-semantics nil)))
   (depict-rtf-to-local-file
    "JS20/RegExpSemantics.rtf"
    "Regular Expression Semantics"
    #'(lambda (rtf-stream)
        (depict-world-commands rtf-stream *rw* :heading-offset 1)))
   (depict-html-to-local-file
    "JS20/RegExpGrammar.html"
    "Regular Expression Grammar"
    t
    #'(lambda (html-stream)
        (depict-world-commands html-stream *rw* :heading-offset 1 :visible-semantics nil))
    :external-link-base "notation.html")
   (depict-html-to-local-file
    "JS20/RegExpSemantics.html"
    "Regular Expression Semantics"
    t
    #'(lambda (html-stream)
        (depict-world-commands html-stream *rw* :heading-offset 1))
    :external-link-base "notation.html")))

#|
(dump-regexp)

(with-local-output (s "JS20/RegExpGrammar.txt") (print-lexer *rl* s) (print-grammar *rg* s))

(lexer-pparse *rl* "a+" :trace t)
(lexer-pparse *rl* "[]+" :trace t)
(run-regexp "(0x|0)2" "0x20")
(run-regexp "(a*)b\\1+c" "aabaaaac")
(run-regexp "(a*)b\\1+c" "aabaabaaaac")
(run-regexp "(a*)b\\1+" "baaaac")
(run-regexp "b(a+)(a+)?(a+)c" "baaaac")
(run-regexp "(((a+)?(b+)?c)*)" "aacbbbcac")
(run-regexp "(\\s\\S\\s)" "aac xa d fds  fds sac")
(run-regexp "(\\s)" "aac xa deac")
(run-regexp "[01234]+aa+" "93-43aabbc")
(run-regexp "[\\101A-ae-]+" "93ABC-@ezy43abc")
(run-regexp "[\\181A-ae-]+" "93ABC-@ezy43abc")
(run-regexp "b[ace]+" "baaaacecfe")
(run-regexp "b[^a]+" "baaaabc")
(run-regexp "(?=(a+))a*b\\1" "baaabac")
(run-regexp "(?=(a+))" "baaabac")
(run-regexp "(.*?)a(?!(a+)b\\2c)\\2(.*)" "baaabaac")
(run-regexp "(aa|aabaac|ba|b|c)*" "aabaac")
(run-regexp "[\\_^01234]+\\_aa+" "93-43aabbc")
(run-regexp "a." "AAab")
(run-regexp "a." "AAab" :ignore-case t)
(run-regexp "a.." (concatenate 'string "a" (string #\newline) "bacd"))
(run-regexp "a.." (concatenate 'string "a" (string #\newline) "bacd") :span t)
|#

#+allegro (clean-grammar *rg*) ;Remove this line if you wish to print the grammar's state tables.
(length (grammar-states *rg*))

#ifdef  __cplusplus
extern "C" {
#endif

extern int VERBOSE;
extern int QUIET;

/* regression-component.c */
void create_new_component(void);
void create_new_component_with_va_args(void);
void create_simple_component(void);
void test_icalcomponent_get_span(void);
void create_new_component_with_va_args(void);

/* regression-classify.c */
void test_classify(void);

/* regression-recur.c */
void test_recur_file(void);

/* regression-cxx.c */
void test_cxx(void);

/* regression-storage.c */
void test_fileset_extended(void);
void test_dirset_extended(void);
void test_bdbset(void);

/* regression-utils.c */
const char* ical_timet_string(const time_t t);
const char* ictt_as_string(struct icaltimetype t);
char* icaltime_as_ctime(struct icaltimetype t);


void _ok(char*name, int result, char*file, int linenum, char *test);
void _is(char* test_name, const char* str1, const char* str2, char *file, int linenum);
void _int_is(char* test_name, int i1, int i2, char *file, int linenum);
#define ok(TEST, EX) (_ok(TEST, EX, __FILE__, __LINE__, #EX))
#define is(S1, S2, EX) (_is(S1, S2, EX, __FILE__, __LINE__))
#define int_is(I1, I2, EX) (_int_is(I1, I2, EX, __FILE__, __LINE__))
void test_header(char*title, int test_set);
void test_start(int);
void test_end(void);
void test_run(char *test_name, 
	      void (*test_fcn)(void), 
	      int do_test, int headeronly);




#ifdef  __cplusplus
}
#endif

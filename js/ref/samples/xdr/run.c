#include <jsapi.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* from common.c */
JSBool Init(JSRuntime **, JSContext **, JSObject **);

char usage[] = "Usage: %s input.jsc\n";

int
main(int argc, char **argv) {
  JSContext *cx;
  JSRuntime *rt;
  JSObject *obj, *global;
  JSString *str;
  jsval args[1], rval;

  char *infile;
  int infd;
  char *buf;
  struct stat sbuf;
  
  if (argc < 2) {
    printf(usage, argv[0]);
    return 1;
  }

  /* 
   * Initialization of the JS engine.  Standard stuff with the usual
   * parameters.
   */
  if (!Init(&rt, &cx, &global)) {
    fprintf(stderr, "failed to initialize JS engine; aborting\n");
    return 1;
  }

  infile = argv[1];
  infd = open(infile, O_RDONLY);
  if (infd < 0) {
    perror("open");
    fprintf(stderr, "failed to open input file %s; aborting\n", infile);
    return 1;
  }
  
  if (fstat(infd, &sbuf) < 0) {
    perror("stat");
    fprintf(stderr, "failed to stat input file %s; aborting\n", infile);
    return 1;
  }

  buf = JS_malloc(cx, sbuf.st_size);
  if (!buf) {
    fprintf(stderr, "failed to allocate input buffer; aborting\n");
    return 1;
  }
  
  /* XXX handle partial reads! */
  if (read(infd, buf, sbuf.st_size) < sbuf.st_size) {
    perror("read");
    fprintf(stderr, "failed to read input; aborting\n");
    return 1;
  }

  /*
   * JS_NewUCStringCopyN et alii create new JSStrings using the provided
   * data as UCS-2 characters.  In this case, we are passing a raw byte stream
   * but we must use the UC variant to prevent the addition of spacer bytes
   * into the data.
   */
  str = JS_NewUCStringCopyN(cx, (jschar *)buf, sbuf.st_size / sizeof(jschar));
  if (!str) {
    fprintf(stderr, "failed to allocate bytecode string; aborting\n");
    return 1;
  }
  
  /*
   * Although there exist (library-private) APIs to the XDR operations, 
   * the supported/documented/kosher APIs are the freeze and thaw methods
   * on the Script object.
   */
  obj = JS_NewScriptObject(cx, NULL);
  if (!obj) {
    fprintf(stderr, "failed to create Script object; aborting\n");
    return 1;
  }

  /*
   * The thaw method replaces the current JSScript private data with 
   * a reconsitituted one based on the XDR-encoded script data provided in
   * string form as argument.
   */
  args[0] = STRING_TO_JSVAL(str);
  if (!JS_CallFunctionName(cx, obj, "thaw", 1, args, &rval)) {
    fprintf(stderr, "thaw operation failed; aborting\n");
    return 1;
  }

  /*
   * The exec method takes an optional Object parameter: the scope in
   * which to execute the script (a la eval).  We pass the global object
   * here to provide access to the standard classes, as well as custom
   * methods like "print".
   */
  args[0] = OBJECT_TO_JSVAL(global);
  if (!JS_CallFunctionName(cx, obj, "exec", 1, args, &rval)) {
    fprintf(stderr, "exec operation failed; aborting\n");
    return 1;
  }

  JS_free(cx, buf);

  return 0;
}

#include <jsapi.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* from common.c */
JSBool Init(JSRuntime **, JSContext **, JSObject **);

char usage[] = "Usage: %s input.js output.jsc\n";

int
main(int argc, char **argv) {
  JSContext *cx;
  JSRuntime *rt;
  JSObject *obj, *global;
  JSScript *script;
  JSString *str;
  jsval rval;

  char *infile, *outfile;
  char *buf;
  int outfd;
  
  if (argc < 3) {
    printf(usage, argv[0]);
    return 1;
  }
  
  if (!Init(&rt, &cx, &global)) {
    fprintf(stderr, "failed to initialized JS engine; aborting\n");
    return 1;
  }

  infile = argv[1];
  outfile = argv[2];
  
  script = JS_CompileFile(cx, global, infile);
  if (!script) {
    fprintf(stderr, "compilation of %s failed; aborting\n", infile);
    return 1;
  }
  
  /*
   * See run.c for information on why we use the Script object.
   */
  obj = JS_NewScriptObject(cx, script);
  if (!obj) {
    fprintf(stderr, "creating of Script object wrapper failed; aborting\n");
    return 1;
  }

  if (!JS_CallFunctionName(cx, obj, "freeze", 0, NULL, &rval)) {
    fprintf(stderr, "freezing of script data failed; aborting\n");
    return 1;
  }

  str = JSVAL_TO_STRING(rval);
  buf = (char *)JS_GetStringChars(str);
  if (!buf) {
    fprintf(stderr, "mysterious string failure; aborting\n");
    return 1;
  }
  
  outfd = open(outfile, O_WRONLY | O_CREAT, 0644);
  if (outfd < 0) {
    perror("open");
    fprintf(stderr, "opening of output stream %s failed; aborting\n", outfile);
    return 1;
  }

  /*
   * Each "character" in a JS String is 2 bytes (sizeof jschar == 2) when
   * accessed via the JS_GetStringChars API, so we have to be sure to
   * account for it here.
   */
  write(outfd, buf, JS_GetStringLength(str) * sizeof(jschar));
  close(outfd);
  
  printf("%s compiled and written to %s\n", infile, outfile);

  return 0;
}

#import <Cocoa/Cocoa.h>

static void SetupRuntimeOptions(int argc, const char *argv[])
{
    if (getenv("MOZ_UNBUFFERED_STDIO")) {
      printf("stdout and stderr unbuffered\n");
      setbuf(stdout, 0);
      setbuf(stderr, 0);
    }
}

int main(int argc, const char *argv[])
{
    SetupRuntimeOptions(argc, argv);
    
    return NSApplicationMain(argc, argv);
}

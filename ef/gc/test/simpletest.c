
/* a simple test -- just allocate an object, set its reference to
   NULL, and run the collector. */

#include "sm.h"
#include "smobj.h"
#include "smtrav.h"

#define K               1024
#define M               (K*K)
#define PAGE            (4*K)
#define MIN_HEAP_SIZE   (4*M)
#define MAX_HEAP_SIZE   (16*M)

typedef struct MyClassStruct {
  SMObjStruct                 super;
  struct MyClassStruct*       next;
  int                         value;
  
} MyClassStruct;

PRUword dumpFlags = SMDumpFlag_Detailed;

static void
MarkRoots(SMGenNum genNum, PRBool copyCycle, void* data)
{
  fprintf (stderr, "MarkRoots called\n");
//    SM_MarkThreadsConservatively(genNum, copyCycle, data);      /* must be first */
  //    SM_MarkRoots((SMObject**)&root, 1, PR_FALSE);
}

static void
MyClass_finalize(SMObject* self)
{
  fprintf(stderr, "finalizing %d\n", SM_GET_FIELD(MyClassStruct, self, value));
}

SMObjectClass* MyClass;

static void
InitClass(void)
{
    SMFieldDesc* fds;

    MyClass = (SMObjectClass*)malloc(sizeof(SMObjectClass)
                                     + (1 * sizeof(SMFieldDesc)));
    SM_InitObjectClass(MyClass, NULL, sizeof(SMObjectClass),
                       sizeof(MyClassStruct), 1, 0);
    fds = SM_CLASS_GET_INST_FIELD_DESCS((SMClass*)MyClass);
    fds[0].offset = offsetof(MyClassStruct, next) / sizeof(void*);
    fds[0].count = 1;
    MyClass->data.finalize = MyClass_finalize;
}

int
main()
{
  PRStatus status;
  MyClassStruct* obj;

  PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
  PR_SetThreadGCAble();
  status = SM_InitGC(MIN_HEAP_SIZE, MAX_HEAP_SIZE, 
		     NULL, NULL,
		     NULL, NULL,
		     MarkRoots, NULL);
  
  if (status != PR_SUCCESS)
    return -1;

  InitClass();

  obj = (MyClassStruct*)SM_AllocObject(MyClass);      /* gets collected */

  if (obj) {
    ((MyClassStruct*)SM_DECRYPT(obj))->value = 1;
  }
  else {
    fprintf(stderr, "ALLOCATION FAILURE\n");
    exit(1);
  }

  SM_DumpGCHeap(stderr, dumpFlags);

  obj = NULL;

  SM_Collect();
  SM_RunFinalization();
  SM_Collect();

  SM_DumpGCHeap(stderr, dumpFlags);
}

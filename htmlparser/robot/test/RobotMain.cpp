#include "nsXPCOM.h"
#include "nsVoidArray.h"
#include "nsString.h"
class nsIWebShell;

extern "C" NS_EXPORT int DebugRobot(nsVoidArray * workList, nsIWebShell * ww);

int main(int argc, char **argv)
{
  nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
  if (NS_FAILED(rv)) {
    printf("NS_InitXPCOM2 failed\n");
    return 1;
  }

  nsVoidArray * gWorkList = new nsVoidArray();
  if(gWorkList) {
    int i;
    for (i = 1; i < argc; ++i) {
      nsString *tempString = new nsString;
      tempString->AssignWithConversion(argv[i]);
      gWorkList->AppendElement(tempString);
    }
  }

  return DebugRobot(gWorkList, nsnull);
}


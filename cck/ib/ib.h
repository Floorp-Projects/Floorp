#define MID_SIZE 512

typedef struct s_xpi {
	CString xpiname;
	CString filename;
	} XPI;
typedef struct s_jar {
	CString jarname;
	CString filename;
	} JAR;

int StartIB(void);
BOOL FillGlobalWidgetArray(CString);
void CreateRshell(void);
void CreateHelpMenu(void);
void CreateBuddyList(void);
void CreateIspMenu(void);
void CreateNewsMenu(void);


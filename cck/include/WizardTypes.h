#ifndef WIZARDTYPES
#define WIZARDTYPES

#define MIN_SIZE 256
#define MID_SIZE 512
#define MAX_SIZE 1024
#define EXTD_MAX_SIZE 10240

// Some global structures
typedef struct NVPAIR
{
	char name[MID_SIZE];
	char value[MID_SIZE];
	char options[MAX_SIZE];
	char type[MID_SIZE];
}NVPAIR;

typedef struct ACTIONSET
{
	CString event;
	CString dll;
	CString function;
	char parameters[MAX_SIZE];
	CString onInit;
	CString onCommand;
}ACTIONSET;

typedef struct DIMENSION
{
	int width;
	int height;
}DIMENSION;

typedef struct FIELDLEN
{
	int length;
	int max_len;
	int min_len;
}FIELDLEN;
typedef struct OPTIONS
{
	char* name[25];
	char* value[25];
}OPTIONS;

typedef struct WIDGET
{
	CString type;
	CString name;
	CString attrib;
	CString value;
	CString display;
	CString title;
	CString group;
	CString target;
	CString description;
	CString showinsection;	// Used in the ShowSection command to show and hide widgets based on selection in a listbox.
	POINT location;
	DIMENSION size;
	FIELDLEN fieldlen;
	ACTIONSET action;
	int numOfOptions;
	OPTIONS options;
	int numOfOptDesc;
	OPTIONS optDesc;
	CString items;
	BOOL cached;
	UINT widgetID;
	CWnd *control;
}WIDGET;


typedef struct IMAGE
{
	CString name;
	CString value;
	POINT location;
	DIMENSION size;	
	HBITMAP hBitmap;
}IMAGE;

typedef struct WIZBUT
{
	CString back;
	CString next;
	CString cancel;
	CString help;
}WIZBUT;

typedef struct VARS
{
	CString title;
	CString caption;
	CString pageName;
	CString image;
	CString visibility;
	CString functionality;
	WIZBUT *wizbut;
}VARS;

typedef struct PAGE
{
	CStringArray pages;
	CStringArray visibility;
}PAGE;

typedef struct CONTROLS
{
	CString onNextAction;
	CString onEnter;
	CString helpFile;
}CONTROLS;

typedef struct WIDGETGROUPS
{
	CString groupName;
	CString widgets;
}WIDGETGROUPS;

typedef struct NODE
{
	NODE *parent;
	NODE **childNodes;
	int numChildNodes;
	int currNodeIndex;
	VARS *localVars;
	PAGE *subPages;
	CONTROLS *navControls;
	WIDGET** pageWidgets;
	int numWidgets;
	int currWidgetIndex;
	int pageBaseIndex;
	IMAGE **images;
	int numImages;
	BOOL nodeBuilt;
	BOOL isWidgetsSorted;
}NODE;

//-----------------------------------------------------------

typedef int (DLLPROC)(CString parms, WIDGET *curWidget);

typedef struct DLLINFO
{
	CString *dllName;
	CString *procName;
	HMODULE hDLL;
	DLLPROC *procAddr;
	DLLINFO *next;
} DLLINFO;

#endif //WIZARDTYPES

#include <Xfe/XfeTest.h>
#include <XmL/Tree.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

static void		load_tree1		(Widget);
static void		load_tree2		(Widget);

static Widget	create_tree1	(Widget,String);
static Widget	create_tree2	(Widget,String);
static Widget	create_tree3	(Widget,String);
static Widget	create_tree4	(Widget,String);

#define NUM_TREES 4

typedef Widget (*foo_create_t)  (Widget,String);

static foo_create_t funcs[NUM_TREES] =
{
	create_tree1,
	create_tree2,
	create_tree3,
	create_tree4
};

/*----------------------------------------------------------------------*/
int
main(int argc,char *argv[])
{
	Widget	frames[4];
	Widget	forms[4];
	Widget	trees[4];
	Cardinal i;

	XfeAppCreate("TreeTest",&argc,argv);

	for(i = 0; i < NUM_TREES; i++)
	{
		frames[i] = XfeFrameCreate(XfeNameIndex("Frame",i + 1),NULL,0);

		forms[i] = XfeDescendantFindByName(frames[i],
										   "MainForm",
										   XfeFIND_ANY,
										   False);

		trees[i] = funcs[i](forms[i],XfeNameIndex("Tree",i + 1));

		XtPopup(frames[i],XtGrabNone);
	}

    XfeAppMainLoop();

	return 0;
}
/*----------------------------------------------------------------------*/
static Widget
create_tree1(Widget pw,String name)
{
	Widget	tree;

	Pixmap pixmap, pixmask;
	XmString str;

	tree = XtVaCreateManagedWidget(name,
								   xmlTreeWidgetClass, 
								   pw,
								   XmNvisibleRows, 10,
								   XmNwidth, 250,
								   NULL);
	
	XtVaSetValues(tree,
		XmNlayoutFrozen, True,
		NULL);

	pixmap = XmUNSPECIFIED_PIXMAP;
	pixmask = XmUNSPECIFIED_PIXMAP;

	str = XmStringCreateSimple("Root");
	XmLTreeAddRow(tree, 0, True, True, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Level 1 Parent");
	XmLTreeAddRow(tree, 1, True, True, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("1st Child of Level 1 Parent");
	XmLTreeAddRow(tree, 2, False, False, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("2nd Child of Level 1 Parent");
	XmLTreeAddRow(tree, 2, False, False, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Level 2 Parent");
	XmLTreeAddRow(tree, 2, True, True, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Child of Level 2 Parent");
	XmLTreeAddRow(tree, 3, False, False, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Level 1 Parent");
	XmLTreeAddRow(tree, 1, True, True, -1, pixmap, pixmask, str);
	XmStringFree(str);
	str = XmStringCreateSimple("Child of Level 1 Parent");
	XmLTreeAddRow(tree, 2, False, False, -1, pixmap, pixmask, str);
	XmStringFree(str);

	XtVaSetValues(tree,
		XmNlayoutFrozen, False,
		NULL);

	return tree;
}
/*----------------------------------------------------------------------*/
static Widget
create_tree2(Widget pw,String name)
{
	Widget	tree;

	int i, n, size;
	XmString str;
	XmLTreeRowDefinition *rows;
	static struct
			{
		Boolean expands;
		int level;
		char *string;
	} data[] =
	{
		{ True,  0, "Root" },
		{ True,  1, "Level 1 Parent" },
		{ False, 2, "1st Child of Level 1 Parent" },
		{ False, 2, "2nd Child of Level 1 Parent" },
		{ True,  2, "Level 2 Parent" },
		{ False, 3, "Child of Level 2 Parent" },
		{ True,  1, "Level 1 Parent" },
		{ False, 2, "Child of Level 1 Parent" },
	};

	tree = XtVaCreateManagedWidget(name,
		xmlTreeWidgetClass, pw,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 6,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
		XtVaTypedArg, XmNselectBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNselectForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNconnectingLineColor, XmRString, "#808080", 8,
		XmNvisibleRows, 10,
		XmNwidth, 250,
		NULL);
	XtVaSetValues(tree,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		NULL);

	/* Create a TreeRowDefinition array from the data array */
	/* and add rows to the Tree */
	n = 8;
	size = sizeof(XmLTreeRowDefinition) * n;
	rows = (XmLTreeRowDefinition *)malloc(size);
	for (i = 0; i < n; i++)
	{
		rows[i].level = data[i].level;
		rows[i].expands = data[i].expands;
		rows[i].isExpanded = True;
		rows[i].pixmap = XmUNSPECIFIED_PIXMAP;
		rows[i].pixmask = XmUNSPECIFIED_PIXMAP;
		rows[i].string = XmStringCreateSimple(data[i].string);
	}
	XmLTreeAddRows(tree, rows, n, -1);

	/* Free the TreeRowDefintion array (and XmStrings) we created above */
	for (i = 0; i < n; i++)
		XmStringFree(rows[i].string);
	free((char *)rows);

	return tree;
}
/*----------------------------------------------------------------------*/
static Widget
create_tree3(Widget pw,String name)
{
#define sphere_width 16
#define sphere_height 16
static unsigned char sphere_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0x38, 0x3f,
	0xb8, 0x3f, 0xf8, 0x3f, 0xf8, 0x3f, 0xf8, 0x3f, 0xf0, 0x1f, 0xe0, 0x0f,
	0xc0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#define monitor_width 16
#define monitor_height 16
static unsigned char monitor_bits[] = {
	0x00, 0x00, 0xf8, 0x3f, 0xf8, 0x3f, 0x18, 0x30, 0x58, 0x37, 0x18, 0x30,
	0x58, 0x37, 0x18, 0x30, 0xf8, 0x3f, 0xf8, 0x3f, 0x80, 0x03, 0x80, 0x03,
	0xf0, 0x1f, 0xf0, 0x1f, 0x00, 0x00, 0x00, 0x00};

	Widget	tree;

	XmLTreeRowDefinition *rows;
	Pixmap monitorPixmap, spherePixmap;
	Pixel black, white;
	int i, n, size;
	static struct
			{
		Boolean expands;
		int level;
		char *string;
	} data[] =
	{
		{ True,  0, "Root" },
		{ True,  1, "Parent A" },
		{ False, 2, "Node A1" },
		{ False, 2, "Node A2" },
		{ True,  2, "Parent B" },
		{ False, 3, "Node B1" },
		{ False, 3, "Node B2" },
		{ True,  1, "Parent C" },
		{ False, 2, "Node C1" },
		{ True,  1, "Parent D" },
		{ False, 2, "Node D1" },
	};

	black = BlackPixelOfScreen(XtScreen(pw));
	white = WhitePixelOfScreen(XtScreen(pw));
	spherePixmap = XCreatePixmapFromBitmapData(XtDisplay(pw),
		DefaultRootWindow(XtDisplay(pw)),
		sphere_bits, sphere_width, sphere_height,
		black, white,
		DefaultDepthOfScreen(XtScreen(pw)));
	monitorPixmap = XCreatePixmapFromBitmapData(XtDisplay(pw),
		DefaultRootWindow(XtDisplay(pw)),
		monitor_bits, monitor_width, monitor_height,
		black, white,
		DefaultDepthOfScreen(XtScreen(pw)));

	/* Create a Tree with 3 columns and 1 heading row in multiple */
	/* select mode.  We also set globalPixmapWidth and height here */
	/* which specifys that every Pixmap we set on the Tree will be */
	/* the size specified (16x16).  This will increase performance. */
	tree = XtVaCreateManagedWidget(name,
		xmlTreeWidgetClass, pw,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
		XtVaTypedArg, XmNselectBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNselectForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNconnectingLineColor, XmRString, "#808080", 8,
		XmNallowColumnResize, True,
		XmNheadingRows, 1,
		XmNvisibleRows, 14,
		XmNcolumns, 3,
		XmNvisibleColumns, 5,
		XmNsimpleWidths, "12c 8c 10c",
		XmNsimpleHeadings, "All Folders|SIZE|DATA2",
		XmNselectionPolicy, XmSELECT_MULTIPLE_ROW,
		XmNhighlightRowMode, True,
		XmNglobalPixmapWidth, 16,
		XmNglobalPixmapHeight, 16,
		NULL);

	/* Set default values for new cells (the cells in the content rows) */
	XtVaSetValues(tree,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XmNcellRightBorderType, XmBORDER_NONE,
		XmNcellTopBorderType, XmBORDER_NONE,
		XmNcellBottomBorderType, XmBORDER_NONE,
		NULL);

	/* Create a TreeRowDefinition array from the data array */
	/* and add rows to the Tree */
	n = 11;
	size = sizeof(XmLTreeRowDefinition) * n;
	rows = (XmLTreeRowDefinition *)malloc(size);
	for (i = 0; i < n; i++)
	{
		rows[i].level = data[i].level;
		rows[i].expands = data[i].expands;
		rows[i].isExpanded = True;
		if (data[i].expands)
			rows[i].pixmap = spherePixmap;
		else
			rows[i].pixmap = monitorPixmap;
		rows[i].pixmask = XmUNSPECIFIED_PIXMAP;
		rows[i].string = XmStringCreateSimple(data[i].string);
	}
	XmLTreeAddRows(tree, rows, n, -1);

	/* Free the TreeRowDefintion array we created above and set strings */
	/* in column 1 and 2 */
	for (i = 0; i < n; i++)
	{
		XmStringFree(rows[i].string);
		XmLGridSetStringsPos(tree, XmCONTENT, i, XmCONTENT, 1, "1032|1123");
	}
	free((char *)rows);

	return tree;
}
/*----------------------------------------------------------------------*/


static void rowExpand(Widget w,XtPointer clientData,XtPointer callData);
static void rowCollapse(Widget w,XtPointer clientData,XtPointer callData);
static void cellSelect(Widget w,XtPointer clientData,XtPointer callData);

static Widget grid;

static Widget
create_tree4(Widget pw,String name)
{
	Widget tree;
	XmString str;

	/* Add Tree to left of Form */
	tree = XtVaCreateManagedWidget(name,
		xmlTreeWidgetClass, pw,
		XmNhorizontalSizePolicy, XmCONSTANT,
		XmNautoSelect, False,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
		XtVaTypedArg, XmNselectBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNselectForeground, XmRString, "white", 6,
		XtVaTypedArg, XmNconnectingLineColor, XmRString, "#808080", 8,
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_POSITION,
		XmNrightPosition, 45,
		NULL);
	XtVaSetValues(tree,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		NULL);

	/* Add a single row containing the root path to the Tree */
	str = XmStringCreateSimple("/");
	XmLTreeAddRow(tree, 1, True, False, 0,
		XmUNSPECIFIED_PIXMAP, XmUNSPECIFIED_PIXMAP, str);
	XmStringFree(str);
	XtVaSetValues(tree,
		XmNrow, 0,
		XmNrowUserData, strdup("/"),
		NULL);

	XtAddCallback(tree, XmNexpandCallback, rowExpand, NULL);
	XtAddCallback(tree, XmNcollapseCallback, rowCollapse, NULL);
	XtAddCallback(tree, XmNselectCallback, cellSelect, NULL);

	/* Add a Grid to the right of the Form and set cell defaults */
	grid = XtVaCreateManagedWidget("grid",
		xmlGridWidgetClass, pw,
		XtVaTypedArg, XmNbackground, XmRString, "#C0C0C0", 8,
		XtVaTypedArg, XmNforeground, XmRString, "black", 6,
		XtVaTypedArg, XmNblankBackground, XmRString, "white", 6,
		XtVaTypedArg, XmNselectBackground, XmRString, "#000080", 8,
		XtVaTypedArg, XmNselectForeground, XmRString, "white", 6,
		XmNcolumns, 3,
		XmNsimpleWidths, "24c 12c 10c",
		XmNsimpleHeadings, "Name|Type|Size",
		XmNvisibleColumns, 6,
		XmNallowColumnResize, True,
		XmNheadingRows, 1,
		XmNvisibleRows, 16,
		XmNhighlightRowMode, True,
		XmNleftAttachment, XmATTACH_POSITION,
		XmNleftPosition, 46,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XtVaTypedArg, XmNcellBackground, XmRString, "white", 6,
		XmNcellTopBorderType, XmBORDER_NONE,
		XmNcellBottomBorderType, XmBORDER_NONE,
		XmNcellRightBorderType, XmBORDER_NONE,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XmNcellAlignment, XmALIGNMENT_LEFT,
		NULL);
	XtVaSetValues(grid,
		XmNcellDefaults, True,
		XmNcolumn, 2,
		XmNcellAlignment, XmALIGNMENT_RIGHT,
		NULL);

	/* Invoke the select callback for the first row in the Tree */
	/* to fill the Grid with the data for the root path */
	XmLGridSelectRow(tree, 0, True);

	return tree;
}
/*----------------------------------------------------------------------*/

static 
void rowExpand(Widget w,XtPointer clientData,XtPointer callData)
{
	XmLGridCallbackStruct *cbs;
	XmLGridRow row;
	int level, pos;
	DIR *dir;
	struct dirent *d;
	struct stat s;
	char *path, fullpath[1024];
	XmString str;

	/* Retrieve the path of the directory expanded.  This is kept */
	/* in the row's rowUserData */
	cbs = (XmLGridCallbackStruct *)callData;
	row = XmLGridGetRow(w, XmCONTENT, cbs->row);
	XtVaGetValues(w,
		XmNrowPtr, row,
		XmNrowUserData, &path,
		XmNrowLevel, &level,
		NULL);
	pos = cbs->row + 1;
	dir = opendir(path);
	if (!dir)
		return;

	/* Add the subdirectories of the directory expanded to the Tree */
	XtVaSetValues(w,
		XmNlayoutFrozen, True,
		NULL);
	while (d = readdir(dir))
	{
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		sprintf(fullpath, "%s/%s", path, d->d_name);
		if (lstat(fullpath, &s) == -1)
			continue;
		if (!S_ISDIR(s.st_mode))
			continue;
		str = XmStringCreateSimple(d->d_name);
		XmLTreeAddRow(w, level + 1, True, False, pos,
			XmUNSPECIFIED_PIXMAP, XmUNSPECIFIED_PIXMAP, str);
		XmStringFree(str);
		XtVaSetValues(w,
			XmNrow, pos,
			XmNrowUserData, strdup(fullpath),
			NULL);
		pos++;
	}
	closedir(dir);
	XtVaSetValues(w,
		XmNlayoutFrozen, False,
		NULL);
}

static 
void rowCollapse(Widget w,XtPointer clientData,XtPointer callData)
{
	XmLGridCallbackStruct *cbs;
	XmLGridRow row;
	char *path;
	int i, j, level, rows;

	/* Collapse the row by deleting the rows in the tree which */
	/* are children of the collapsed row.  We also need to free */
	/* the path contained in the rowUserData of the rows deleted */
	cbs = (XmLGridCallbackStruct *)callData;
	row = XmLGridGetRow(w, XmCONTENT, cbs->row);
	XtVaGetValues(w,
		XmNrowPtr, row,
		XmNrowLevel, &level,
		NULL);
	XtVaGetValues(w,
		XmNrows, &rows,
		NULL);
	i = cbs->row + 1;
	while (i < rows)
	{
		row = XmLGridGetRow(w, XmCONTENT, i);
		XtVaGetValues(w,
			XmNrowPtr, row,
			XmNrowLevel, &j,
			XmNrowUserData, &path,
			NULL);
		if (j <= level)
			break;
		free(path);
		i++;
	}
	j = i - cbs->row - 1;
	XmLGridDeleteRows(w, XmCONTENT, cbs->row + 1, j);
}

static
void cellSelect(Widget w,XtPointer clientData,XtPointer callData)
{
	XmLGridCallbackStruct *cbs;
	XmLGridRow row;
	DIR *dir;
	struct stat s;
	struct dirent *d;
	char *path, fullpath[1024], buf[256];
	int pos;

	/* Retrieve the directory selected */
	cbs = (XmLGridCallbackStruct *)callData;
	if (cbs->rowType != XmCONTENT)
		return;
	row = XmLGridGetRow(w, XmCONTENT, cbs->row);
	XtVaGetValues(w,
		XmNrowPtr, row,
		XmNrowUserData, &path,
		NULL);
	dir = opendir(path);
	if (!dir)
		return;

	/* Add a row for each file in the directory to the Grid */
	pos = 0;
	XtVaSetValues(grid,
		XmNlayoutFrozen, True,
		NULL);
	XmLGridDeleteAllRows(grid, XmCONTENT);
	while (d = readdir(dir))
	{
		sprintf(fullpath, "%s/%s", path, d->d_name);
		if (lstat(fullpath, &s) == -1)
			continue;
		XmLGridAddRows(grid, XmCONTENT, pos, 1);
		XmLGridSetStringsPos(grid, XmCONTENT, pos, XmCONTENT, 0, d->d_name);
		if (S_ISDIR(s.st_mode))
			sprintf(buf, "Directory");
		else if (S_ISLNK(s.st_mode))
			sprintf(buf, "Link");
		else
			sprintf(buf, "File");
		XmLGridSetStringsPos(grid, XmCONTENT, pos, XmCONTENT, 1, buf);
		sprintf(buf, "%d", (int)s.st_size);
		XmLGridSetStringsPos(grid, XmCONTENT, pos, XmCONTENT, 2, buf);
		pos++;
	}
	closedir(dir);
	XtVaSetValues(grid,
		XmNlayoutFrozen, False,
		NULL);
}


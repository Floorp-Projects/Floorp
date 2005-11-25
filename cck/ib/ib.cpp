
// Includes
#include "stdafx.h"
#include <Winbase.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include "globals.h"
#include "comp.h"
#include "ib.h"
#include "fstream.h"
#include <afxtempl.h>
#include <afxdisp.h>
#include "resource.h"
#include "NewDialog.h"
#define MAX_SIZE 1024
#define CRVALUE 0x0D
#define BUF_SIZE 4096

// for the xml parser used for PrefsTree
#include "xmlparse.h"
#include "prefselement.h"

// Required disk space for Linux build
#define LDISK_SPACE 84934656

int interpret(char *cmd);

CString rootPath;
CString configName;
CString configPath;
CString workspacePath;
CString shellPath;
CString cdPath;
CString networkPath;
CString tempPath;
CString iniDstPath;
CString iniSrcPath;
CString scriptPath;
CString nscpxpiPath;
CString cdshellPath;
CString outputPath; 
CString xpiDstPath;
CString remoteAdminFile;
CString curPlatform;
CString platformPath;
CString curLanguage;

// variables for CCK Linux build
CString templinuxPath;
CString templinuxDirPath;
CString xpiDir;
CString templinuxDir;
CString targzfile;
CString strNscpInstaller = "./netscape-installer";

// For use with PrefsTree
CString gstrCFGPrefFile;
CString gstrPlainPrefFile;
CString gstrInstallFile;

WIDGET *tempWidget;
char buffer[50000];
XPI	xpiList[100];
int xpiLen = -1;
JAR jarList[100];
int jarLen = -1;
// Setup Sections for config.ini
CString Setup0Short = "R&ecommended";
CString Setup1Short = "C&ustom";
CString quotes = "\"";
CString spaces = " ";
BOOL prefDoesntExist = TRUE;
COMPONENT Components[100];
int		numComponents;
int		componentOrder;
CString componentstr;
char tempcomponentstr[MAX_SIZE];

int findXPI(CString xpiname, CString filename)
{
	int found = FALSE;

	for (int i=0; !found && i<=xpiLen; i++)
	//xpiList is an array of structures where each structure contains 
	//the name of the xpi file and the subpath of the file within.
		if (xpiList[i].xpiname == xpiname && xpiList[i].filename == filename)
			found = TRUE;

	if (!found)
	{
		xpiLen++;
		xpiList[xpiLen].xpiname  = xpiname;
		xpiList[xpiLen].filename = filename;
	}

	return found;
}

int ExtractXPIFile(CString xpiname, CString xpifile)
{

  CString command;
//	AfxMessageBox("The xpiname is "+xpiname+" and the file is "+xpifile,MB_OK);
	if (findXPI(xpiname, xpifile))
		return TRUE;

	// Can use -d instead of change CWD???
	CString xpiArchive = nscpxpiPath + "\\" + xpiname; //xpiArchive = CCKTool\NSCPXPI
	command = quotes +rootPath + "unzip.exe"+ quotes + "-o" + spaces + quotes + xpiArchive + quotes + spaces + quotes + xpifile + quotes;
	ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);

	return TRUE;
}

int ReplaceXPIFiles()
{
	CString command;
	CString xpiArchive;
	CString xpiArcDest;

	// Go through the whole list putting them into the archives
	for (int i=0; i<=xpiLen; i++)
	{
		// This copy preserves the existing archive if it exists - do we
		// need to delete it the first time through?
		xpiArchive = nscpxpiPath + "\\" + xpiList[i].xpiname;//nscpxpiPath=CCKTool\NSCPXPI
		xpiArcDest = xpiDstPath + "\\" + xpiList[i].xpiname; //xpiDstPath=CCKTool\Configs\ configName\Output\Core
		if (!CopyFile(xpiArchive, xpiArcDest, TRUE))
			DWORD e = GetLastError();
//		if ((strcmp(xpiList[i].filename,"bin/defaults/isp/US") == 0) || (strcmp(xpiList[i].filename,"bin/defaults/isp") == 0))
		if (((xpiList[i].filename).Find("isp")) != -1)
			command = quotes + rootPath + "zip.exe" + quotes + "-m " + spaces + quotes +xpiArcDest + quotes + spaces + quotes + xpiList[i].filename + "/*.*" + quotes;
		else
			command = quotes + rootPath + "zip.exe" + quotes + "-m " + spaces + quotes +xpiArcDest + quotes + spaces + quotes + xpiList[i].filename + quotes;
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
	}

	return TRUE;
}

int findJAR(CString jarname, CString filename)
{
	int found = FALSE;

	for (int i=0; !found && i<=jarLen; i++)
		if (jarList[i].jarname == jarname && 
			jarList[i].filename == filename)
				found = TRUE;

	if (!found)
	{
		jarLen++;
		jarList[jarLen].jarname  = jarname;
		jarList[jarLen].filename = filename;
	}

	return found;
}

// Decrypt a file to another file.
int UnHash(CString HashedFile, CString ClearTextFile)
{
	CString command = quotes + rootPath + "parse_cfg.exe" + quotes + " -Y -input " + HashedFile + " -output " + ClearTextFile;
	ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);

	return TRUE;
}

// Encrypt a file to another file.
int Hash(CString ClearTextFile, CString HashedFile)
{
	CString command = quotes + rootPath + "make_cfg.exe" + quotes + 
		" -Y -input " + quotes + ClearTextFile + quotes +" -output " + 
		quotes + HashedFile + quotes;
	ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);

	return TRUE;
}

int ExtractJARFile(CString xpiname, CString jarname, CString xpifile)
{
//	AfxMessageBox("The xpiname is "+xpiname+" and the jar name is "+jarname+" and the file is "+xpifile,MB_OK);

	ExtractXPIFile(xpiname, jarname);

	CString command;
	CString jarArchive;
	
	if (curPlatform == "Mac OS")
	{
		jarname.Replace("/","\\");
		jarname.Insert(0,"\\");
		jarArchive = tempPath + jarname;
	}
	else
	{
		//We have to get rid of the bin/chrome/ and hence the delete.
		jarname.Delete(0,11);
		jarArchive = tempPath + "\\bin\\chrome\\" + jarname;
	}

	if (findJAR(jarname, xpifile))
		return TRUE;
	
	// Can use -d instead of change CWD???
	command = quotes +rootPath + "unzip.exe"+ quotes + "-o" + spaces + quotes + jarArchive + quotes + spaces + quotes + xpifile + quotes;
	ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);

	return TRUE;
}

int ReplaceJARFiles()
{
	CString command;
	CString jarArchive;

	// Go through the whole list putting them into the archives
	for (int i=0; i<=jarLen; i++)
	{
		// This copy preserves the existing archive if it exists - do we
		// need to delete it the first time through?
		if (curPlatform == "Mac OS")
			jarArchive = tempPath + jarList[i].jarname;
		else
			jarArchive = tempPath + "\\bin\\chrome\\" + jarList[i].jarname;

		command = quotes + rootPath + "zip.exe" + quotes + "-m " + spaces + quotes +jarArchive + quotes + spaces + quotes + jarList[i].filename + quotes;
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
	}

	return TRUE;
}

int ReplaceINIFile()
{
	CString command1,command2,commandReadme;
	CString zipName("NSSetup.zip");
	CString exeName("NSSetup.exe");
	CString copyb = "copy /b ";
	char	olddir[1024];

	GetCurrentDirectory(sizeof(olddir), olddir);

	if(SetCurrentDirectory((char *)(LPCTSTR) xpiDstPath) == FALSE)
		return FALSE;

	CString Src = nscpxpiPath + "\\" +zipName;
	CString Dst = xpiDstPath + "\\" + zipName;
	if (!CopyFile(Src, Dst, FALSE))
		DWORD e = GetLastError();

//	command = quotes + rootPath + "nszip.exe " + quotes + spaces + exeName + spaces + "config.ini";
	command1 = quotes + rootPath + "zip.exe" + quotes + "-m " + spaces + zipName + spaces + "config.ini";
	ExecuteCommand((char *)(LPCTSTR) command1, SW_HIDE, INFINITE);
//	Copy the Readme file to the core directory and insert it inside the NSSetup.zip**********
	CString readmePath = GetGlobal("ReadMeFile");
	CString replaceReadme = xpiDstPath + "\\Readme.txt";
	if (!CopyFile(readmePath, replaceReadme, FALSE))
		DWORD e = GetLastError();
	commandReadme = quotes + rootPath + "zip.exe" + quotes + "-m " + spaces + zipName + spaces + "Readme.txt";
	ExecuteCommand((char *)(LPCTSTR) commandReadme, SW_HIDE, INFINITE);
// Finished copying readme and inserting into the zip**********
	command2 = copyb + quotes + rootPath + "unzipsfx.exe" + quotes + " + NSSetup.zip NSSetup.exe";
//	copy /b unzipsfx.exe+letters.zip letters.exe
	///////////////////////////////////////////////////////

	CString copycat = "copycat.bat";
	ofstream cc(copycat);
	cc << command2 <<"\necho off \ncls\n";
	cc.close();
	CString command3 ="copycat.bat";

	///////////////////////////////////////////////////////
	ExecuteCommand((char *)(LPCTSTR) command3, SW_HIDE, INFINITE);

	DeleteFile("copycat.bat");
	CString unzipN6 = quotes +rootPath + "unzip.exe"+ quotes + "-o" + spaces + quotes + zipName + quotes;
	ExecuteCommand((char *)(LPCTSTR) unzipN6, SW_HIDE, INFINITE);
	DeleteFile(zipName);

	SetCurrentDirectory(olddir);

	return TRUE;
}


void ModifyPref(char *buffer, CString entity, CString newvalue, BOOL bLockPref)
{
	CString buf(buffer);
	
	int i = buf.Find(entity);
	if (i == -1) return;

	i = buf.Find('"', i+1);
	if (i == -1) return;
	i++;

	i = buf.Find('"', i+1);
	if (i == -1) return;
	i++;

	int j = buf.Find('"', i);
	if (j == -1) return;
	j--;

	buf.Delete(i, j-i+1);
	buf.Insert(i, newvalue);

	if (bLockPref)
	{
		// If it's not lockPref( already.
		if (buf.Find("lockPref(") < 0)
			buf.Replace("pref(", "lockPref(");
	}

	strcpy(buffer, (char *)(LPCTSTR) buf);
	prefDoesntExist = FALSE;
}

int ModifyProperties(CString xpifile, CString entity, CString newvalue)
{

	int rv = TRUE;
	CString propFile = xpifile;
	CString tempFile = xpifile + ".temp";
	char properties[400];

	ofstream tf(tempFile);
	if(!tf) 
	{
		rv = FALSE;
		cout <<"Cannot open: "<<tempFile<<"\n";
		return rv;
	}

	tf<< entity <<"="<<newvalue <<"\n";

	ifstream pf(propFile);
	if (!pf)
	{
		rv = FALSE;
		cout <<"Cannot open: "<<propFile<<"\n";
		return rv;
	}
	
	while (!pf.eof()) 
	{
		pf.getline(properties,400);
		tf <<properties<<"\n";
	}
	pf.close();
	tf.close();
	remove(xpifile);
	rename(tempFile, xpifile);
	return rv;
}
void AddPref(CString xpifile, CString entity, CString newvalue, BOOL bUseQuotes, BOOL bLockPref)
{

	int rv = TRUE;
	CString prefFile = xpifile;
	CString tempFile = xpifile + ".temp";
	char properties[400];

	ofstream tf(tempFile);
	if(!tf) 
	{
		rv = FALSE;
		cout <<"Cannot open: "<<tempFile<<"\n";
		return;
	}

	ifstream pf(prefFile);
	if (!pf)
	{
		rv = FALSE;
		cout <<"Cannot open: "<<prefFile<<"\n";
		return;
	}
	
	while (!pf.eof()) 
	{
		pf.getline(properties,400);
		tf <<properties<<"\n";
	}

	CString Quote = bUseQuotes? quotes : "";
	CString FuncName = bLockPref? "lockPref(" : "pref(";
	tf<< FuncName << entity << ", " << Quote << newvalue << Quote << ");\n";

	pf.close();
	tf.close();
	remove(xpifile);
	rename(tempFile, xpifile);
	return;
}

int ModifyJS(CString xpifile, CString entity, CString newvalue, BOOL bLockPref)
{

	int rv = TRUE;
	CString newfile = xpifile + ".new";
	char *fgetsrv;

	// Read in DTD file and make substitutions
	FILE *srcf = fopen(xpifile, "r");
	FILE *dstf = fopen(newfile, "w");
	CString apost = '"';
	entity.Insert(0,apost);
	entity.Insert(1000,apost);

	if (!srcf)
		rv = FALSE;
	else
	{
		prefDoesntExist = TRUE;
		while (!feof(srcf))
		{
			fgetsrv = fgets(buffer, sizeof(buffer), srcf);
			if (!fgetsrv || ferror(srcf))
			{
				rv = FALSE;
				break;
			}
			ModifyPref(buffer, entity, newvalue, bLockPref);
			fputs(buffer, dstf);
		}

		fclose(srcf);
		fclose(dstf);
	}
	remove(xpifile);
	rename(newfile, xpifile);
	if (prefDoesntExist)
		AddPref(xpifile,entity,newvalue, TRUE, bLockPref);

	return TRUE;
}

void ModifyEntity(char *buffer, CString entity, CString newvalue)
{
	CString buf(buffer);
	entity = entity + " ";  // Ensure we don't get false matches

	int i = buf.Find(entity);
	if (i == -1) return;

	i = buf.Find('"', i+1);
	if (i == -1) return;
	i++;

	int j = buf.Find('"', i);
	if (j == -1) return;
	j--;

	buf.Delete(i, j-i+1);
	buf.Insert(i, newvalue);

	strcpy(buffer, (char *)(LPCTSTR) buf);
}

int ModifyDTD(CString xpifile, CString entity, CString newvalue)
{
	CString newfile = xpifile + ".new";
	int rv = TRUE;
	char *fgetsrv;

	// Read in DTD file and make substitutions
	FILE *srcf = fopen(xpifile, "r");
	FILE *dstf = fopen(newfile, "w");
	if (!srcf)
		rv = FALSE;
	else
	{
		while (!feof(srcf))
		{
			fgetsrv = fgets(buffer, sizeof(buffer), srcf);
			if (!fgetsrv || ferror(srcf))
			{
				rv = FALSE;
				break;
			}
			ModifyEntity(buffer, entity, newvalue);
			fputs(buffer, dstf);
		}

		fclose(srcf);
		fclose(dstf);
	}

	remove(xpifile);
	rename(newfile, xpifile);

	return TRUE;
}

void ModifyEntity1(char *buffer, CString entity, CString newvalue, BOOL bLockPref)
{
	CString buf(buffer);
	entity = entity + "\"";

	int i = buf.Find(entity);
	if (i == -1) return;

	i = buf.ReverseFind('"');
	if (i == -1) return;
	
	CString tempbuf = buf;
	tempbuf.Left(i);
	int j = tempbuf.ReverseFind('"');
	if (j == -1) return;
	
	buf.Delete(j, i-j);
	buf.Insert(j, newvalue);

	if (bLockPref)
	{
		// If it's not LockPref( already, change Pref( to LockPref(.
		if (buf.Find("lockPref(") < 0)
			buf.Replace("pref(", "lockPref(");
	}

	strcpy(buffer, (char *)(LPCTSTR) buf);
}

int ModifyJS1(CString xpifile, CString entity, CString newvalue, BOOL bLockPref)
{
	CString newfile = xpifile + ".new";
	int rv = TRUE;
	char *fgetsrv;

	// Read in all.js file and make substitutions
	FILE *srcf = fopen(xpifile, "r");
	FILE *dstf = fopen(newfile, "w");
	if (!srcf)
		rv = FALSE;
	else
	{
		while (!feof(srcf))
		{
			fgetsrv = fgets(buffer, sizeof(buffer), srcf);
			if (!fgetsrv || ferror(srcf))
			{
				rv = FALSE;
				break;
			}
			ModifyEntity1(buffer, entity, newvalue, bLockPref);
			fputs(buffer, dstf);
		}

		fclose(srcf);
		fclose(dstf);
	}

	remove(xpifile);
	rename(newfile, xpifile);

	return TRUE;
}

void ModifyEntity2(char *buffer, CString entity, CString newvalue, BOOL bLockPref)
{
	CString buf(buffer);
	newvalue = "              " + newvalue;

	int i = buf.Find(entity);
	if (i == -1) return;

	i = buf.ReverseFind(')');
	if (i == -1) return;
	
	CString tempbuf = buf;
	tempbuf.Left(i);
	int j = tempbuf.ReverseFind(',');
	if (j == -1) return;
	
	buf.Delete(j+1, i-j-1);
	buf.Insert(j+1, newvalue);

	if (bLockPref)
	{
		// If it's not LockPref( already.
		if (buf.Find("lockPref(") < 0)
			buf.Replace("pref(", "lockPref(");
	}

	strcpy(buffer, (char *)(LPCTSTR) buf);
	prefDoesntExist = FALSE;
}

int ModifyJS2(CString xpifile, CString entity, CString newvalue, BOOL bLockPref)
{
	CString newfile = xpifile + ".new";
	int rv = TRUE;
	char *fgetsrv;

	// Read in all.js file and make substitutions
	FILE *srcf = fopen(xpifile, "r");
	FILE *dstf = fopen(newfile, "w");
	if (!srcf)
		rv = FALSE;
	else
	{
		prefDoesntExist = TRUE;
		while (!feof(srcf))
		{
			fgetsrv = fgets(buffer, sizeof(buffer), srcf);
			if (!fgetsrv || ferror(srcf))
			{
				rv = FALSE;
				break;
			}
			ModifyEntity2(buffer, entity, newvalue, bLockPref);
			fputs(buffer, dstf);
		}

		fclose(srcf);
		fclose(dstf);
	}

	remove(xpifile);
	rename(newfile, xpifile);
	if (prefDoesntExist)
	{
		// AddPref expects pref name to be surrounded in quotes. 
		CString prefName = "\"" + entity + "\"";
		AddPref(xpifile, prefName, newvalue, FALSE, bLockPref);
	}

	return TRUE;
}

BOOL FileExists(CString file)
{
	WIN32_FIND_DATA data;
	HANDLE d = FindFirstFile((const char *) file, &data);
	return (d != INVALID_HANDLE_VALUE);
}

BOOL CreateNewFile(CString& Filename, CString Contents)
{
		FILE *f = fopen(Filename, "w");
		if (!f)
			return FALSE;
		fprintf(f,Contents);
		fclose(f);

		return TRUE;
}

// Modifies a preference in a hashed prefs file.
// Specify whether the pref value should be "string", "bool", or "int".
// Adds the pref if it is missing.
// Creates the file if missing.
// Returns TRUE if OK, or FALSE on error.
int ModifyHashedPref(CString HashedPrefsFile, CString PrefName, CString NewPrefValue, CString PrefType, BOOL bLockPref)
{
	// Unhash the prefs file to a plain text file. If there is no hashed file yet,
	// create a plaintext file with only a comment.
	CString PlainTextPrefsFile = HashedPrefsFile + ".js";
	if (FileExists(HashedPrefsFile))
	{
		if (!UnHash(HashedPrefsFile, PlainTextPrefsFile))
			return FALSE;
	}
	else
	{
		// Create a plain text prefs with only a comment.
		CreateNewFile(PlainTextPrefsFile, "/* prefs configured in NCADM */\n");
	}
	
	// Modify the pref.
	if (PrefType.CompareNoCase("string") == 0)
	{
		if (!ModifyJS(PlainTextPrefsFile, PrefName, NewPrefValue, bLockPref))
			return FALSE;
	}
	else if (PrefType.CompareNoCase("int") == 0)
	{
		if (NewPrefValue.IsEmpty())
			NewPrefValue = "0";

		if (!ModifyJS2(PlainTextPrefsFile, PrefName, NewPrefValue, bLockPref))
			return FALSE;
	}
	else if (PrefType.CompareNoCase("bool") == 0)
	{
		if (NewPrefValue.IsEmpty() || NewPrefValue == "0" || (NewPrefValue.CompareNoCase("false") == 0))
			NewPrefValue = "false";
    else
      NewPrefValue = "true";

		if (!ModifyJS2(PlainTextPrefsFile, PrefName, NewPrefValue, bLockPref))
			return FALSE;
	}

	// And rehash it.
	if (!Hash(PlainTextPrefsFile, HashedPrefsFile))
		return FALSE;

	return TRUE;
}


// This processes a prefs tree XML file, adding preferences to install files
// as specified in the prefs tree XML file. See PrefsTree.html for file
// format details.
//

// Called by the XML parser when a new element is read.
void startElement(void *userData, const char *name, const char **atts)
{
  ASSERT(userData);

  ((CPrefElement*)userData)->startElement(name, atts);
}

void characterData(void *userData, const XML_Char *s, int len)
{
  ASSERT(userData);
  ((CPrefElement*)userData)->characterData(s, len);
}


// Called by the XML parser when an element close tag is read.
void endElement(void *userData, const char *name)
{
  ASSERT(userData);
   
  ((CPrefElement*)userData)->endElement(name);

  if (stricmp(name, "PREF") == 0)
  {
    
    // If locked, write to the .cfg file. Don't care if it's default or not.
    
    BOOL bLocked      = ((CPrefElement*)userData)->IsLocked();
    BOOL bRemoteAdmin = ((CPrefElement*)userData)->IsRemoteAdmin();
    BOOL bDefault     = ((CPrefElement*)userData)->IsDefault();

    if (bLocked && !bRemoteAdmin)
    {
      // Write the pref element to prefs file.
      ExtractXPIFile(gstrInstallFile, gstrCFGPrefFile);
      ModifyHashedPref(gstrCFGPrefFile, ((CPrefElement*)userData)->GetPrefName(), ((CPrefElement*)userData)->GetPrefValue(), ((CPrefElement*)userData)->GetPrefType(), TRUE); 
    }

    // If not locked, and not NS default, write to the .js file.
    else if (!bDefault)
    {
      ExtractXPIFile(gstrInstallFile, gstrPlainPrefFile);

      if ((((CPrefElement*)userData)->GetPrefType() == "int") || (((CPrefElement*)userData)->GetPrefType() == "bool"))
        ModifyJS2(gstrPlainPrefFile, ((CPrefElement*)userData)->GetPrefName(), ((CPrefElement*)userData)->GetPrefValue(), FALSE);

      else  // string
        ModifyJS(gstrPlainPrefFile, ((CPrefElement*)userData)->GetPrefName(), ((CPrefElement*)userData)->GetPrefValue(), FALSE);
    }

    // If remote admin, write to the .jsc file.
    if (bRemoteAdmin)
    {
      if (!FileExists(remoteAdminFile))
      {
        CString strURL = GetGlobal("RemoteAdminURL");
        CString strComment;
        strComment.Format("/* This Remote Admin file should be placed at %s */\n", strURL);
        CreateNewFile(remoteAdminFile, strComment);
      }

      if ((((CPrefElement*)userData)->GetPrefType() == "int") || (((CPrefElement*)userData)->GetPrefType() == "bool"))
        ModifyJS2(remoteAdminFile, ((CPrefElement*)userData)->GetPrefName(), ((CPrefElement*)userData)->GetPrefValue(), TRUE);

      else  // string
        ModifyJS(remoteAdminFile, ((CPrefElement*)userData)->GetPrefName(), ((CPrefElement*)userData)->GetPrefValue(), TRUE);

    }
  }

}

BOOL ProcessMailNews() //CString xpifile, browser.xpi,bin/netscp.cfg,bin/defaults/pref/all-ns.js)
{

	BOOL bMailLocked      = (atoi(GetGlobal("MailAcctLocked")) == 0 ? FALSE : TRUE);
	BOOL bMailRemoteAdmin = (atoi(GetGlobal("MailAcctRemoteAdmin")) == 0 ? FALSE : TRUE);
	BOOL bNewsLocked      = (atoi(GetGlobal("NewsAcctLocked")) == 0 ? FALSE : TRUE);
	BOOL bNewsRemoteAdmin = (atoi(GetGlobal("NewsAcctRemoteAdmin")) == 0 ? FALSE : TRUE);

	CString strMailAcctID			= GetGlobal("MailAcctID");
	CString strMailAcctDisplayName	= GetGlobal("MailAcctDisplayName");
	CString strMailAcctServer		= GetGlobal("MailAcctServer");
	CString strMailAcctPortNumber   = GetGlobal("MailAcctPortNumber");
	int       nMailAcctPortNumber   = atoi(GetGlobal("MailAcctPortNumber"));
	CString strMailAcctSMTPServer   = GetGlobal("MailAcctSMTPServer");

	CString strNewsAcctID			= GetGlobal("NewsAcctID");
	CString strNewsAcctDisplayName	= GetGlobal("NewsAcctDisplayName");
	CString strNewsAcctServer		= GetGlobal("NewsAcctServer");
	CString strNewsAcctPortNumber   = GetGlobal("NewsAcctPortNumber");
	int       nNewsAcctPortNumber   = atoi(GetGlobal("NewsAcctPortNumber"));

	BOOL bMailComplete =	strMailAcctID.GetLength()
						 && strMailAcctDisplayName.GetLength()
						 && strMailAcctServer.GetLength()
						 && strMailAcctPortNumber.GetLength()
						 && strMailAcctSMTPServer.GetLength();
	
	BOOL bNewsComplete =    strNewsAcctID.GetLength()
						 && strNewsAcctDisplayName.GetLength()
						 && strNewsAcctServer.GetLength();

    CString strAccountsToAppend;

    // If either is remote admin, ensure .jsc file exists already

    if ((bMailComplete && bMailRemoteAdmin) || (bNewsComplete && bNewsRemoteAdmin))
    {
      if (!FileExists(remoteAdminFile))
      {
        CString strURL = GetGlobal("RemoteAdminURL");
        CString strComment;
        strComment.Format("/* This Remote Admin file should be placed at %s */\n", strURL);
        CreateNewFile(remoteAdminFile, strComment);
      }
	}


	if (bMailComplete)
	{
		// for Mail, we are currently not locking SMTP server info (okay per PM JeffH)
		// and most setup prefs go into all-ns.js unless locked or remote.
		
        // generate account, identity, and server strings from account ID the administrator gave us

		CString strAccount		= "Acct"   + strMailAcctID;
		CString strIdentity		= "id"     + strMailAcctID;
		CString strServer		= "server" + strMailAcctID;
		CString strSMTPServer	= "smtp"   + strMailAcctID;

		CString strPref;
		CString strMailAcctServerType;
		CString strMailAcctPopLeaveMsg;
		BOOL    bIsPop = FALSE;

		strAccountsToAppend = strAccount;  // will add this to appendaccounts pref


		// Determine whether the server type is POP or IMAP
		if (GetGlobal("pop") == "1")
		{
			strMailAcctServerType = "pop3";
			bIsPop = TRUE;

			// check if "leave pop messages on server" option is set
			if (GetGlobal("PopMessages") == "0")	
				strMailAcctPopLeaveMsg = "false";
			else 
				strMailAcctPopLeaveMsg = "true";
		}
		else
			strMailAcctServerType = "imap";

        // these go to the .js file

		ExtractXPIFile(gstrInstallFile, gstrPlainPrefFile);

		strPref.Format("mail.account.%s.identities",strAccount);  // i.e. mail.account.Acct9991.identities
        ModifyJS(gstrPlainPrefFile, strPref, strIdentity, FALSE);

		strPref.Format("mail.account.%s.server",strAccount);	  // i.e. mail.account.Acct9991.server
        ModifyJS(gstrPlainPrefFile, strPref, strServer, FALSE);
 
        strPref.Format("mail.identity.%s.smtpServer",strIdentity);// i.e. mail.identity.id9991.smtpServer
        ModifyJS(gstrPlainPrefFile, strPref, strSMTPServer, FALSE);

        strPref.Format("mail.identity.%s.smtpRequiresUsername",strIdentity);// i.e. mail.identity.id9991.smtpRequiresUsername
        ModifyJS2(gstrPlainPrefFile, strPref, "true", FALSE);

        strPref.Format("mail.smtpserver.%s.hostname",strSMTPServer);   // i.e. mail.smtpserver.smtp9991.hostname
        ModifyJS(gstrPlainPrefFile, strPref, strMailAcctSMTPServer, FALSE);

        ModifyJS(gstrPlainPrefFile, "mail.smtpservers.appendsmtpservers", strSMTPServer, FALSE);
        ModifyJS(gstrPlainPrefFile, "mail.smtp.defaultserver",		 	  strSMTPServer, FALSE);


        strPref.Format("mail.identity.%s.valid",strIdentity);	  // i.e. mail.identity.id9991.valid
        ModifyJS2(gstrPlainPrefFile, strPref, "false", FALSE);

        strPref.Format("mail.identity.%s.wizardSkipPanels",strIdentity);   // i.e. mail.identity.id9991.wizardSkipPanels
        ModifyJS2(gstrPlainPrefFile, strPref, "true", FALSE);


		strPref.Format("mail.server.%s.hostname",strServer);  // i.e. mail.server.server9991.hostname
        ModifyJS(gstrPlainPrefFile, strPref, strMailAcctServer, FALSE);


        if (bMailLocked && !bMailRemoteAdmin)   // save in cfg file
		{
			ExtractXPIFile(gstrInstallFile, gstrCFGPrefFile);

			// user realhostname instead of hostname when locking it

			strPref.Format("mail.server.%s.realhostname",strServer);  // i.e. mail.server.server9991.realhostname
			ModifyHashedPref(gstrCFGPrefFile, strPref, strMailAcctServer, "string", TRUE); 

			strPref.Format("mail.server.%s.name",strServer);  // i.e. mail.server.server9991.name
			ModifyHashedPref(gstrCFGPrefFile, strPref, strMailAcctDisplayName, "string", TRUE); 

			strPref.Format("mail.server.%s.port",strServer);  // i.e. mail.server.server9991.port
			ModifyHashedPref(gstrCFGPrefFile, strPref, strMailAcctPortNumber, "int", TRUE); 

			strPref.Format("mail.server.%s.type",strServer);  // i.e. mail.server.server9991.type
			ModifyHashedPref(gstrCFGPrefFile, strPref, strMailAcctServerType, "string", TRUE); 

			if (bIsPop)
			{
				strPref.Format("mail.server.%s.leave_on_server",strServer);  // i.e. mail.server.server9991.type
				ModifyHashedPref(gstrCFGPrefFile, strPref, strMailAcctPopLeaveMsg, "bool", TRUE); 
			}

		}
		else 
		{
			// if remoteadmin, save in .jsc file, otherwise save in .js file

            CString strTarget = bMailRemoteAdmin ? remoteAdminFile : gstrPlainPrefFile;
			BOOL bLocked      = bMailRemoteAdmin ? TRUE : FALSE;

            // only use .realhostname if pref is locked/remote, otherwise use .hostname

			if (bMailRemoteAdmin)
				strPref.Format("mail.server.%s.realhostname",strServer);  // i.e. mail.server.server9991.realhostname
			else
				strPref.Format("mail.server.%s.hostname",strServer);  // i.e. mail.server.server9991.hostname

            ModifyJS(strTarget, strPref, strMailAcctServer, bLocked);

			strPref.Format("mail.server.%s.name",strServer);  // i.e. mail.server.server9991.name
            ModifyJS(strTarget, strPref, strMailAcctDisplayName, bLocked);

			strPref.Format("mail.server.%s.port",strServer);  // i.e. mail.server.server9991.port
            ModifyJS2(strTarget, strPref, strMailAcctPortNumber, bLocked);

			strPref.Format("mail.server.%s.type",strServer);  // i.e. mail.server.server9991.type
            ModifyJS(strTarget, strPref, strMailAcctServerType, bLocked);

			if (bIsPop)
			{
				strPref.Format("mail.server.%s.leave_on_server",strServer);  // i.e. mail.server.server9991.type
				ModifyJS2(strTarget, strPref, strMailAcctPopLeaveMsg, bLocked); 
			}
		
		}

	}

	if (bNewsComplete)
	{
		// setup prefs go into all-ns.js unless locked or remote.
		
        // generate account, identity, and server strings from account ID the administrator gave us

		CString strAccount		= "Acct"   + strNewsAcctID;
		CString strIdentity		= "id"     + strNewsAcctID;
		CString strServer		= "server" + strNewsAcctID;

		CString strPref;
		CString strMailAcctServerType;

		if (strAccountsToAppend.GetLength())  // the mail account is already there, comma delimit it
			strAccountsToAppend += ",";

		strAccountsToAppend += strAccount;  // will add this to appendaccounts pref


        // these go to the .js file

		ExtractXPIFile(gstrInstallFile, gstrPlainPrefFile);

		strPref.Format("mail.account.%s.identities",strAccount);  // i.e. mail.account.Acct9992.identities
        ModifyJS(gstrPlainPrefFile, strPref, strIdentity, FALSE);

		strPref.Format("mail.account.%s.server",strAccount);	  // i.e. mail.account.Acct9992.server
        ModifyJS(gstrPlainPrefFile, strPref, strServer, FALSE);
 
        strPref.Format("mail.identity.%s.valid",strIdentity);	  // i.e. mail.identity.id9992.valid
        ModifyJS2(gstrPlainPrefFile, strPref, "false", FALSE);

        strPref.Format("mail.identity.%s.wizardSkipPanels",strIdentity);   // i.e. mail.identity.id9992.wizardSkipPanels
        ModifyJS2(gstrPlainPrefFile, strPref, "true", FALSE);

	    strPref.Format("mail.server.%s.type",strServer);   // i.e. mail.server.server9992.type
		ModifyJS(gstrPlainPrefFile, strPref, "nntp", FALSE);

		strPref.Format("mail.server.%s.hostname",strServer);  // i.e. mail.server.server9991.hostname
        ModifyJS(gstrPlainPrefFile, strPref, strNewsAcctServer, FALSE);


        if (bNewsLocked && !bNewsRemoteAdmin)   // save in cfg file
		{
			ExtractXPIFile(gstrInstallFile, gstrCFGPrefFile);

			// user realhostname instead of hostname when locking it

			strPref.Format("mail.server.%s.realhostname",strServer);  // i.e. mail.server.server9992.realhostname
			ModifyHashedPref(gstrCFGPrefFile, strPref, strNewsAcctServer, "string", TRUE); 

			strPref.Format("mail.server.%s.name",strServer);		  // i.e. mail.server.server9992.name
			ModifyHashedPref(gstrCFGPrefFile, strPref, strNewsAcctDisplayName, "string", TRUE); 

			if (strNewsAcctPortNumber.GetLength())
			{
				strPref.Format("mail.server.%s.port",strServer);	  // i.e. mail.server.server9992.port
				ModifyHashedPref(gstrCFGPrefFile, strPref, strNewsAcctPortNumber, "int", TRUE); 
			}

		}
		else 
		{
			// if remoteadmin, save in .jsc file, otherwise save in .js file

            CString strTarget = bNewsRemoteAdmin ? remoteAdminFile : gstrPlainPrefFile;
			BOOL bLocked      = bMailRemoteAdmin ? TRUE : FALSE;

            // only use .realhostname if pref is locked/remote, otherwise use .hostname

			if (bNewsRemoteAdmin)
				strPref.Format("mail.server.%s.realhostname",strServer);  // i.e. mail.server.server9992.realhostname
			else
				strPref.Format("mail.server.%s.hostname",strServer);  // i.e. mail.server.server9992.hostname

            ModifyJS(strTarget, strPref, strNewsAcctServer, bLocked);

			strPref.Format("mail.server.%s.name",strServer);  // i.e. mail.server.server9991.name
            ModifyJS(strTarget, strPref, strNewsAcctDisplayName, bLocked);

			if (strNewsAcctPortNumber.GetLength())
			{
				strPref.Format("mail.server.%s.port",strServer);  // i.e. mail.server.server9991.port
				ModifyJS2(strTarget, strPref, strNewsAcctPortNumber, bLocked);
			}

		}

	}

	if (strAccountsToAppend.GetLength())
		ModifyJS(gstrPlainPrefFile, "mail.accountmanager.appendaccounts", strAccountsToAppend, FALSE);

	return TRUE;
}

	
	
// special case for homepage URL: if locked or remote admin, do the right thing.
// otherwise, call the originally called ModifyProperties method

BOOL ModifyHomepageURL(CString xpifile, CString entity, CString newvalue)
{

  bool bLocked      = GetGlobal("HomePageURLLocked") == "1";
  bool bRemoteAdmin = GetGlobal("HomePageURLRemoteAdmin") == "1";

  CString strPref   = "browser.startup.homepage";
  CString strURL    = GetGlobal("HomePageURL");

  if (bLocked && !bRemoteAdmin)
  {
    // Write the pref element to prefs file.
    ExtractXPIFile(gstrInstallFile, gstrCFGPrefFile);
    ModifyHashedPref(gstrCFGPrefFile, strPref, strURL, "string", TRUE); 

    // Lock out the buttons in the pref dialog to change the homepage.
    ModifyHashedPref(gstrCFGPrefFile, "pref.browser.homepage.disable_button.current_page", "true", "bool", TRUE);
    ModifyHashedPref(gstrCFGPrefFile, "pref.browser.homepage.disable_button.select_file", "true", "bool", TRUE);
  }
   
  // If remote admin, write to the .jsc file.

  else if (bRemoteAdmin)
  {
    if (!FileExists(remoteAdminFile))
    {
      CString strComment;
      strComment.Format("/* The Homepage URL should be %s */\n", strURL);
      CreateNewFile(remoteAdminFile, strComment);
    }

    ModifyJS(remoteAdminFile, strPref, strURL, TRUE);

    // Lock out the buttons in the pref dialog to change the homepage.
    ModifyJS2(remoteAdminFile, "pref.browser.homepage.disable_button.current_page", "true", TRUE);
    ModifyJS2(remoteAdminFile, "pref.browser.homepage.disable_button.select_file", "true", TRUE);

  }

  // not locked, or remote, just modify the property
  else
    ModifyProperties(xpifile, entity, newvalue);

  return TRUE;
}



// Sets the autoadmin.global_config_url in strPrefFile to strURL.
BOOL ConfigureRemoteAdmin(CString strURL, CString strPrefFile)
{
  ASSERT(!strURL.IsEmpty() && !strPrefFile.IsEmpty());
  if (strURL.IsEmpty() || strPrefFile.IsEmpty())
    return FALSE;

  ASSERT(strPrefFile.Find(".cfg") > 0);
  if (strPrefFile.Find(".cfg") <= 0)
    return FALSE;
    

  ModifyHashedPref(strPrefFile, "autoadmin.global_config_url", strURL, "string", TRUE); 

  CString strFailoverCached = GetGlobal("RemoteAdminFailoverToCached");
  ModifyHashedPref(strPrefFile, "autoadmin.failover_to_cached", strFailoverCached, "bool", TRUE); 

  CString strFailover = GetGlobal("RemoteAdminFailover");
  ModifyHashedPref(strPrefFile, "autoadmin.offline_failover", strFailover, "bool", TRUE); 

  CString strPoll = GetGlobal("RemoteAdminPoll");
  if (strPoll == "1")
  {
    CString strPollFreq = GetGlobal("RemoteAdminPollFreq");
    ModifyHashedPref(strPrefFile, "autoadmin.refresh_interval", strPollFreq, "int", TRUE); 
  }
  

  return TRUE;
}

// This function can easily be rewriten to parse the XML file by hand if it 
// needs to be ported to a non-MS OS. The XML is pretty simple.
BOOL ProcessPrefsTree(CString strPrefsTreeFile)
{

  //AfxMessageBox("Pause", MB_OK);
  XML_Parser parser = XML_ParserCreate(NULL);

  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, characterData);

  // Used by the XML parser. The data object passed to the handlers.
  static CPrefElement element;
  XML_SetUserData(parser, &element);

  // Load the XML from the file.
  CString strPrefsXML;
  FILE* pFile = fopen(strPrefsTreeFile, "r");
  if (!pFile)
  {
    fprintf(stderr, "Can't open the file %s.", strPrefsTreeFile);
    return FALSE;
  }

  // obtain file size.
  fseek(pFile , 0 , SEEK_END);
  long lSize = ftell(pFile);
  rewind(pFile);

  // allocate memory to contain the whole file.
  char* buffer = (char*) malloc (lSize + 1);
  if (buffer == NULL)
  {
    fprintf(stderr, "Memory allocation error.");
    return FALSE;
  }

  buffer[lSize] = '\0';

  // copy the file into the buffer.
  size_t len = fread(buffer,1,lSize,pFile);
  
  // the whole file is loaded in the buffer. 

  int done = 0;
  if (!XML_Parse(parser, buffer, len, done)) 
  {
    fprintf(stderr,
	    "%s at line %d\n",
	    XML_ErrorString(XML_GetErrorCode(parser)),
	    XML_GetCurrentLineNumber(parser));

    return FALSE;  
  }

  XML_ParserFree(parser);
  free(buffer);
  
  return TRUE;
}


BOOL ModifyUserJS(CString HashedPrefsFile, CString jsSourceFile)
{

	// Unhash the prefs file to a plain text file. If there is no hashed file yet,
	// create a plaintext file with only a comment.
	CString PlainTextPrefsFile = HashedPrefsFile + ".js";
	if (FileExists(HashedPrefsFile))
	{
		if (!UnHash(HashedPrefsFile, PlainTextPrefsFile))
			return FALSE;
	}
	else
	{
		// Create a plain text prefs with only a comment.
		CreateNewFile(PlainTextPrefsFile, "/* prefs configured in NCADM */\n");
	}
	
  // find the block and replace it with the contents of the source file

	CString newPrefsFile = PlainTextPrefsFile + ".new";

	// Read in all.js file and make substitutions
  CStdioFile srcJSC;

  FILE* destJSC = fopen(newPrefsFile, "w");

  if (srcJSC.Open(PlainTextPrefsFile,CFile::modeRead | CFile::typeText) && destJSC)
  {
    
    CString  strLine;
    bool     bInJSBlock = FALSE;
    bool     bInsertUserJSNow = FALSE;
    bool     bDroppedPayload = FALSE;

    int      iBraceLevel = 0;                // these are in case cfg file has braces
    bool     bPastFirstBrace = FALSE;


    while (srcJSC.ReadString(strLine))
    {
   
      strLine += "\n";

      // count braces

/*
      char buffer[4096];

      strcpy(buffer,strLine);
      char *token = strtok( buffer, "{");
      while( token != NULL )
      {
        bPastFirstBrace = TRUE;
        iBraceLevel++;
        token = strtok(NULL,"{");
      }
   
      strcpy(buffer,strLine);
      token = strtok( buffer, "}");
      while( token != NULL )
      {
        iBraceLevel--;
        token = strtok(NULL,"}");
      }

      if (bPastFirstBrace && iBraceLevel < 1)
        bInsertUserJSNow = TRUE;
*/
      
      // looking for //ADMJS_BEG (ADM JavaScript Begin)  
      //          or //ADMJS_END (ADM JavaScript End)        which must be on a line by themselves


      int len = strLine.GetLength();

      if (   strLine.GetLength() > 10 
          && strLine[0] == '/' 
          && strLine[1] == '/')
      {

        CString str = strLine.Left(6);

        if (str.CompareNoCase("\n\n//ADMJS_BEG\n") == 0)
        {
          bInsertUserJSNow = TRUE;
          bInJSBlock = TRUE;
        }
        else if (str.CompareNoCase("//ADMJS_END\n") == 0)
          bInJSBlock = FALSE;
      }
       
      
      // drop our payload
      //
      if (bInsertUserJSNow)
      {
        CStdioFile srcJS;
        CString strJSLine;

        fputs(strLine,destJSC);  // write out "//ADMJS_BEG"

        if (srcJS.Open(jsSourceFile,CFile::modeRead | CFile::typeText))
        {
          while (srcJS.ReadString(strJSLine))
          {
            strJSLine += "\n";
    				fputs(strJSLine, destJSC);
          }
          srcJS.Close();
        }
        bInsertUserJSNow = FALSE;
        bDroppedPayload = TRUE;
      }
      else
      {
        fputs(strLine,destJSC);  // drop whatever line we have
      }

    }  // while source lines

    if (!bDroppedPayload)
    {
      CStdioFile srcJS;
      CString strJSLine;

      strJSLine = "\n\n//ADMJS_BEG\n";
      fputs(strJSLine,destJSC);  

      if (srcJS.Open(jsSourceFile,CFile::modeRead | CFile::typeText))
      {
        while (srcJS.ReadString(strJSLine))
        {
          strJSLine += "\n";
  				fputs(strJSLine, destJSC);
        }
        srcJS.Close();
      }

      strJSLine = "//ADMJS_END\n";
      fputs(strJSLine,destJSC);  

    }


    srcJSC.Close();
    fclose(destJSC);

  }  // if can open source file

  // delete orig and rename new file to correct name

	remove(PlainTextPrefsFile);
	rename(newPrefsFile, PlainTextPrefsFile);
  
	// And rehash it.
	if (!Hash(PlainTextPrefsFile, HashedPrefsFile))
		return FALSE;


	return TRUE;
}


BOOL MarkCFGVersion(CString HashedPrefsFile, CString versionTxt)
{

	// Unhash the prefs file to a plain text file. If there is no hashed file yet,
	// create a plaintext file with only a comment.
	CString PlainTextPrefsFile = HashedPrefsFile + ".js";
	if (FileExists(HashedPrefsFile))
	{
		if (!UnHash(HashedPrefsFile, PlainTextPrefsFile))
			return FALSE;
	}
	else
	{
		// Create a plain text prefs with only a comment.
		CreateNewFile(PlainTextPrefsFile, "/* prefs configured in NCADM */\n");
	}
	
  // find the block and replace it with the contents of the source file

	CString newPrefsFile = PlainTextPrefsFile + ".new";

	// Read in all.js file and make substitutions
  CStdioFile srcJSC;

  FILE* destJSC = fopen(newPrefsFile, "w");

  if (srcJSC.Open(PlainTextPrefsFile,CFile::modeRead | CFile::typeText) && destJSC)
  {
    
    CString  strLine;

    strLine = "//ADMVERSION:" + versionTxt + "\n";
    fputs(strLine,destJSC);

    while (srcJSC.ReadString(strLine))
    {
   
      strLine += "\n";

      CString str = strLine.Left(8);
      if (str.Compare("//ADMVER") == 0)     // throw away any previous //ADMVERSION tags
        continue;

      fputs(strLine,destJSC);  // drop whatever line we have
    }  


    srcJSC.Close();
    fclose(destJSC);

  }  // if can open source file

  // delete orig and rename new file to correct name

	remove(PlainTextPrefsFile);
	rename(newPrefsFile, PlainTextPrefsFile);
  
	// And rehash it.
	if (!Hash(PlainTextPrefsFile, HashedPrefsFile))
		return FALSE;


	return TRUE;
}

void ModifySidebar(CString inputFile, CString outputFile)
{
	// Add a sidebar tab at the topmost location in 'My sidebar'

	char tempbuf[MAX_SIZE];
	CString searchstr = "<RDF:Seq>";
	CString sidebartabTitle = GetGlobal("SidebartabTitle");
	CString sidebartabURL = GetGlobal("SidebartabURL");

	ifstream srcf(inputFile);
	if (!srcf)
	{
		AfxMessageBox("Cannot open input file: " + CString(inputFile),MB_OK);
		return;
	}       
	ofstream dstf(outputFile); 
	if (!dstf)
	{
		AfxMessageBox("Cannot open output file: " + CString(outputFile), MB_OK);
		return;
	}

	while (!srcf.eof()) 
	{
		srcf.getline(tempbuf,MAX_SIZE);
		dstf << tempbuf << "\n";
		if ((CString(tempbuf).Find(searchstr)) != -1)
		{
			if (!(sidebartabTitle.IsEmpty()) && !(sidebartabURL.IsEmpty()))
			{
				dstf << "        <RDF:li>" << "\n";
				dstf << "         <RDF:Description about=\"" << sidebartabTitle << "\">\n";
				dstf << "          <NC:title>" << sidebartabTitle << "</NC:title>" << "\n";
				dstf << "          <NC:content>" << sidebartabURL << "</NC:content>" << "\n";
				dstf << "         </RDF:Description>" << "\n";
				dstf << "        </RDF:li>" << "\n";
			}
		}
	}
	
	srcf.close();
	dstf.close();
}

void ModifyBookmarks(CString inputFile, CString outputFile)
// add custom bookmark(s) either at the top or bottom of the bookmark menu
{
	char tempbuf[MAX_SIZE];

	// variables for custom personal toolbar
	CString toolbarURL1      = GetGlobal("ToolbarURL1");
	CString toolbarURL2      = GetGlobal("ToolbarURL2");
	CString toolbarURL3      = GetGlobal("ToolbarURL3");
	CString toolbarTitle1    = GetGlobal("ToolbarTitle1");
	CString toolbarTitle2    = GetGlobal("ToolbarTitle2");
	CString toolbarTitle3    = GetGlobal("ToolbarTitle3");
	CString toolbarSearchStr = ">Personal Toolbar Folder<";

	// variables for custom bookmark
	CString bkmkChoice         = GetGlobal("radioGroup3");
	CString bkmkLocation       = GetGlobal("BookmarkLocation");
	CString firstbkmkSearchStr = "<h1>Bookmarks</h1>";
	CString lastbkmkSearchStr  = ">Travel<";
	CString folderTitle        = GetGlobal("FolderTitle");
	CString bookmarkTitle      = GetGlobal("BookmarkTitle");
	CString bookmarkTitle1     = GetGlobal("BookmarkTitle1");
	CString bookmarkTitle2     = GetGlobal("BookmarkTitle2");
	CString bookmarkTitle3     = GetGlobal("BookmarkTitle3");
	CString bookmarkTitle4     = GetGlobal("BookmarkTitle4");
	CString bookmarkTitle5     = GetGlobal("BookmarkTitle5");
	CString bookmarkURL        = GetGlobal("BookmarkURL");
	CString bookmarkURL1       = GetGlobal("BookmarkURL1");
	CString bookmarkURL2       = GetGlobal("BookmarkURL2");
	CString bookmarkURL3       = GetGlobal("BookmarkURL3");
	CString bookmarkURL4       = GetGlobal("BookmarkURL4");
	CString bookmarkURL5       = GetGlobal("BookmarkURL5");

	ifstream srcf(inputFile);
	if (!srcf)
	{
		AfxMessageBox("Cannot open input file: " + CString(inputFile),MB_OK);
		return;
	}       
	ofstream dstf(outputFile); 
	if (!dstf)
	{
		AfxMessageBox("Cannot open output file: " + CString(outputFile), MB_OK);
		srcf.close();
		return;
	}

	if (curLanguage == "enus")
	// begin bookmark customization for US builds
	{
		while (!srcf.eof()) 
		{
			srcf.getline(tempbuf,MAX_SIZE);
			dstf << tempbuf << "\n";
		
			if ((CString(tempbuf).Find(toolbarSearchStr)) != -1)
			// add custom personal toolbar items
			{
				srcf.getline(tempbuf,MAX_SIZE);
				dstf << tempbuf << "\n";
				if (!(toolbarTitle1.IsEmpty()) && !(toolbarURL1.IsEmpty()))
					dstf << " <dt><a HREF=\"" << toolbarURL1 << "\">" << toolbarTitle1 << "</a>\n";
				if (!(toolbarTitle2.IsEmpty()) && !(toolbarURL2.IsEmpty()))
					dstf << " <dt><a HREF=\"" << toolbarURL2 << "\">" << toolbarTitle2 << "</a>\n";
				if (!(toolbarTitle3.IsEmpty()) && !(toolbarURL3.IsEmpty()))
					dstf << " <dt><a HREF=\"" << toolbarURL3 << "\">" << toolbarTitle3 << "</a>\n";
			}
	
			if (bkmkChoice == "1")
			// add single custom bookmark
			{
				if ( ((bkmkLocation == "First") && ((CString(tempbuf).Find(firstbkmkSearchStr)) != -1)) ||
					 ((bkmkLocation == "Last") && ((CString(tempbuf).Find(lastbkmkSearchStr)) != -1)) )
				{
					srcf.getline(tempbuf,MAX_SIZE);
					dstf << tempbuf << "\n";
					if (bkmkLocation == "Last")
					{
						// read one more line if bookmark location is 'last'
						srcf.getline(tempbuf,MAX_SIZE);
						dstf << tempbuf << "\n";
					}
					if (!(bookmarkTitle.IsEmpty()) && !(bookmarkURL.IsEmpty()))
						dstf << " <dt><a HREF=\"" << bookmarkURL << "\">" << bookmarkTitle << "</a>\n";
				}
			}
			else if (bkmkChoice == "2")
			// add custom bookmark folder
			{
				if ( ((bkmkLocation == "First") && ((CString(tempbuf).Find(firstbkmkSearchStr)) != -1)) ||
					 ((bkmkLocation == "Last") && ((CString(tempbuf).Find(lastbkmkSearchStr)) != -1)) )
				{
					srcf.getline(tempbuf,MAX_SIZE);
					dstf << tempbuf << "\n";
					
					if (bkmkLocation == "Last")
					{
						// read one more line if bookmark location is 'last'
						srcf.getline(tempbuf,MAX_SIZE);
						dstf << tempbuf << "\n";
					}

					if (!(folderTitle.IsEmpty()))
					{
						dstf << "<dt><h3>" << folderTitle << "</h3>\n";
						dstf << "<dl><p>\n";
						if (!(bookmarkTitle1.IsEmpty()) && !(bookmarkURL1.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL1 << "\">" << bookmarkTitle1 << "</a>\n";
						if (!(bookmarkTitle2.IsEmpty()) && !(bookmarkURL2.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL2 << "\">" << bookmarkTitle2 << "</a>\n";
						if (!(bookmarkTitle3.IsEmpty()) && !(bookmarkURL3.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL3 << "\">" << bookmarkTitle3 << "</a>\n";
						if (!(bookmarkTitle4.IsEmpty()) && !(bookmarkURL4.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL4 << "\">" << bookmarkTitle4<< "</a>\n";
						if (!(bookmarkTitle5.IsEmpty()) && !(bookmarkURL5.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL5 << "\">" << bookmarkTitle5 << "</a>\n";
						dstf << "</dl><p>\n";
					}
				}
			}
		}
	} //end bookmark customization for US builds

	else
	// begin bookmark customization for non-US builds
	{
		toolbarSearchStr = "custom personal toolbar";
		firstbkmkSearchStr = "custom bookmark start";
		lastbkmkSearchStr  = "custom bookmark end";
		while (!srcf.eof()) 
		{
			srcf.getline(tempbuf,MAX_SIZE);
			dstf << tempbuf << "\n";
		
			if ((CString(tempbuf).Find(toolbarSearchStr)) != -1)
			// add custom personal toolbar items
			{
				srcf.getline(tempbuf,MAX_SIZE);
				dstf << tempbuf << "\n";
				if (!(toolbarTitle1.IsEmpty()) && !(toolbarURL1.IsEmpty()))
					dstf << " <dt><a HREF=\"" << toolbarURL1 << "\">" << toolbarTitle1 << "</a>\n";
				if (!(toolbarTitle2.IsEmpty()) && !(toolbarURL2.IsEmpty()))
					dstf << " <dt><a HREF=\"" << toolbarURL2 << "\">" << toolbarTitle2 << "</a>\n";
				if (!(toolbarTitle3.IsEmpty()) && !(toolbarURL3.IsEmpty()))
					dstf << " <dt><a HREF=\"" << toolbarURL3 << "\">" << toolbarTitle3 << "</a>\n";
			}
	
			if (bkmkChoice == "1")
			// add single custom bookmark
			{
				if ( ((bkmkLocation == "First") && ((CString(tempbuf).Find(firstbkmkSearchStr)) != -1)) ||
					 ((bkmkLocation == "Last") && ((CString(tempbuf).Find(lastbkmkSearchStr)) != -1)) )
				{
					srcf.getline(tempbuf,MAX_SIZE);
					dstf << tempbuf << "\n";

					if (!(bookmarkTitle.IsEmpty()) && !(bookmarkURL.IsEmpty()))
						dstf << " <dt><a HREF=\"" << bookmarkURL << "\">" << bookmarkTitle << "</a>\n";
				}
			}
			else if (bkmkChoice == "2")
			// add custom bookmark folder
			{
				if ( ((bkmkLocation == "First") && ((CString(tempbuf).Find(firstbkmkSearchStr)) != -1)) ||
					 ((bkmkLocation == "Last") && ((CString(tempbuf).Find(lastbkmkSearchStr)) != -1)) )
				{
					srcf.getline(tempbuf,MAX_SIZE);
					dstf << tempbuf << "\n";
					
					if (!(folderTitle.IsEmpty()))
					{
						dstf << "<dt><h3>" << folderTitle << "</h3>\n";
						dstf << "<dl><p>\n";
						if (!(bookmarkTitle1.IsEmpty()) && !(bookmarkURL1.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL1 << "\">" << bookmarkTitle1 << "</a>\n";
						if (!(bookmarkTitle2.IsEmpty()) && !(bookmarkURL2.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL2 << "\">" << bookmarkTitle2 << "</a>\n";
						if (!(bookmarkTitle3.IsEmpty()) && !(bookmarkURL3.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL3 << "\">" << bookmarkTitle3 << "</a>\n";
						if (!(bookmarkTitle4.IsEmpty()) && !(bookmarkURL4.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL4 << "\">" << bookmarkTitle4<< "</a>\n";
						if (!(bookmarkTitle5.IsEmpty()) && !(bookmarkURL5.IsEmpty()))
							dstf << " <dt><a HREF=\"" << bookmarkURL5 << "\">" << bookmarkTitle5 << "</a>\n";
						dstf << "</dl><p>\n";
					}
				}
			}
		}
	} // end bookmark customization for non-US builds

	srcf.close();
	dstf.close();
}

void ModifyHelpMenu(CString inputFile, CString outputFile)
{
	// Add an item to the Help menu

	char tempbuf[MAX_SIZE];
	CString searchstr = "oncommand=\"openTopWin(\'urn:clienturl:help:security\');\" />";
	CString helpMenuName = GetGlobal("HelpMenuCommandName");
	CString helpMenuUrl = GetGlobal("HelpMenuCommandURL");

	ifstream srcf(inputFile);
	if (!srcf)
	{
		AfxMessageBox("Cannot open input file: " + CString(inputFile),MB_OK);
		return;
	}       
	ofstream dstf(outputFile); 
	if (!dstf)
	{
		AfxMessageBox("Cannot open output file: " + CString(outputFile), MB_OK);
		srcf.close();
		return;
	}

	while (!srcf.eof()) 
	{
		if (curPlatform == "Mac OS")
			srcf.getline(tempbuf,MAX_SIZE,'\r');
		else
			srcf.getline(tempbuf,MAX_SIZE);
		dstf << tempbuf << "\n";
		if ((CString(tempbuf).Find(searchstr)) != -1)
		{
			if (!(helpMenuName.IsEmpty()) && !(helpMenuUrl.IsEmpty()))
			{
				dstf << "     <menuitem label=\"" << helpMenuName << "\"\n"; 
				dstf << "        position=\"7\"\n"; 
				dstf << "        oncommand=\"openTopWin('" << helpMenuUrl << "');\" />\n";
			}
		}
	}
	
	srcf.close();
	dstf.close();
}

BOOL RemoveNIMRestart(CString inputFile, CString outputFile)
{

        char tempbuf[MAX_SIZE];
	BOOL bCheck0 = FALSE;
	BOOL bCheck1 = FALSE;
	BOOL bCheck2 = FALSE; 
	BOOL bCheck3 = FALSE;
	BOOL bCheck4 = FALSE;

        ifstream srcf(inputFile);
        if (!srcf)
        {
                AfxMessageBox("Cannot open input file: " + CString(inputFile),MB_OK);
                return FALSE;
        }
        ofstream dstf(outputFile);
        if (!dstf)
        {
                AfxMessageBox("Cannot open output file: " + CString(outputFile), MB_OK);
		srcf.close();
                return FALSE;
        }
	srcf.getline(tempbuf,MAX_SIZE);
        while (!srcf.eof())
        {
		if ((CString(tempbuf).Find("var bTurboSet;")) != -1)
		{
			bCheck0 = TRUE;

			dstf << "// The following has been commented out by your Customization Software\n";
			dstf << "if(0)\n";
			dstf << "{\n";
			dstf << tempbuf << "\n";
			dstf << "} //End of Customization change\n";

			srcf.getline(tempbuf, MAX_SIZE);
			while (!srcf.eof())
			{
				dstf << tempbuf << "\n";

				// Search for start of block of code we want to comment out
              			if ((CString(tempbuf).Find("// Register Aim to quick launch")) != -1)
               			{ 
					bCheck1 = TRUE;
			
					//Found start of block of code
					//Add comments to describe what we are doing, and add if(0)
					dstf << "// The following has been commented out by your Customization Software\n";
					dstf << "if(0)\n";
					dstf << "{\n";
 	
					srcf.getline(tempbuf,MAX_SIZE);
					while (!srcf.eof())
					{
						dstf << tempbuf << "\n";
						if(((CString(tempbuf).Find("if(")) != -1) && ((CString(tempbuf).Find("AIM")) != -1))
						{
							bCheck2 = TRUE;

							srcf.getline(tempbuf, MAX_SIZE);
							while (!srcf.eof())
							{
								dstf << tempbuf << "\n";
								if((CString(tempbuf).Find("gWinReg.setValueString(subkey, valname, newKey);")) != -1)
								{
									bCheck3 = TRUE;

									srcf.getline(tempbuf, MAX_SIZE);
									while (!srcf.eof())
									{
										dstf << tempbuf << "\n";
										if((CString(tempbuf).Find("}")) != -1)
										{
											bCheck4 = TRUE;

											dstf << "} //End of Customization change\n";
											break;
										}
										srcf.getline(tempbuf, MAX_SIZE);
									}
								}
								srcf.getline(tempbuf, MAX_SIZE);
							}
						}
						srcf.getline(tempbuf, MAX_SIZE);
					}
				}
				srcf.getline(tempbuf, MAX_SIZE);
			}
		}
		else
			dstf << tempbuf << "\n";
		srcf.getline(tempbuf, MAX_SIZE);
	}

			
        srcf.close();
        dstf.close();

	if(!(bCheck0 && bCheck1 && bCheck2 && bCheck3 && bCheck4))
	{
		//We didn't pass all the code checkpoints
		//return FALSE and leave the file unchanged	
		AfxMessageBox("Code change to remove NIM Restart feature was unsuccessful, leaving file unchanged", MB_OK);
		return FALSE;
	}

	return TRUE;

}
	 

int interpret(char *cmd)
{
	char *cmdname = strtok(cmd, "(");

	if (strcmp(cmdname, "configure") == 0)
	{
		char temp[1024];
		char *section	= strtok(NULL, ",)");
		char *key 		= strtok(NULL, ",)");
		char *value 	= strtok(NULL, ",)");
		char *newvalue	= value;
		if (value[0] == '%')
		{
			value++;
			char *t = strchr(value, '%');
//Post Beta - we have to figure a way to handle these cases - right now returning FALSE
//causes the other commands to not be carried through- changing return FALSE to TRUE
			if (!t)
				return TRUE;//*** Changed FALSE to TRUE
			*t = '\0';
			newvalue = (char *)(LPCTSTR) GetGlobal(value);
		}
		if (!section || !key || !newvalue)
			return TRUE;//***Changed FALSE to TRUE
		if (!CopyFile(iniSrcPath, iniDstPath, TRUE))
			DWORD e = GetLastError();
		if (strcmp(key, "Program Folder Name") == 0)
		{
			CString browserName = GetGlobal("BrowserName");
			if (strcmp(newvalue, "") == 0)
				strcpy(temp, browserName);
			else
			{
				browserName = browserName + " by ";
				strcpy(temp, browserName);
			}
			strcat(temp, newvalue);
			newvalue = temp;
		}
		CString encodedValue = newvalue;
		WritePrivateProfileString(section, key, ConvertUTF8toANSI(encodedValue), 
			iniDstPath);
	}
  else if (strcmp(cmdname, "configureAddText") == 0)
  {
 		char *fileToAdd	= strtok(NULL, ",)");

    // copy config.ini to destination if it hasn't already been done
		if (!CopyFile(iniSrcPath, iniDstPath, TRUE))
			DWORD e = GetLastError();

    CStdioFile sfConfig, sfAddText;
    CString strAddTextFile = rootPath + fileToAdd;

    if (sfConfig.Open(iniDstPath, CFile::modeReadWrite | CFile::typeText) == 0)
      return TRUE;
    
    if (sfAddText.Open(strAddTextFile, CFile::modeRead | CFile::typeText) == 0)
    {
      sfConfig.Close();
      return TRUE;
    }

    sfConfig.SeekToEnd();
    CString strLine;

    while(sfAddText.ReadString(strLine))
    {
      sfConfig.WriteString(strLine + '\n');
    }

    sfAddText.Close();
    sfConfig.Flush();
    sfConfig.Close();
  }
	else if (strcmp(cmdname, "replaceXPI") == 0)
	{
		char *xpiname	= strtok(NULL, ",)");
		char *jname		= strtok(NULL, ",)");
		char *xpifile	= strtok(NULL, ",)");
		char *value 	= strtok(NULL, ",)");
		char *newvalue	= value;
		CString jarname = jname;
		if (value[0] == '%')
		{
			value++;
			char *t = strchr(value, '%');
			if (!t)
				return TRUE;//*** Changed FALSE to TRUE
			*t = '\0';
			newvalue = (char *)(LPCTSTR) GetGlobal(value);
		}
		if (!xpiname || !xpifile || !newvalue)
				return TRUE;//*** Changed FALSE to TRUE

		/*Post Beta -This is a hack to prevent the last page from staying up there endlessly;
		  We check to see if the filename is null and if it is, we return true 
		  so that the return value isn't made FALSE */
		CString filename = newvalue;
		if (filename.IsEmpty())
			return TRUE;
		////////////////////////////////
		//check to see if it is a jar and then do accordingly
		if (jarname.CompareNoCase("no.jar")==0)
		ExtractXPIFile(xpiname, xpifile);
		else 
		ExtractJARFile(xpiname, jarname, xpifile);

		if (!CopyFile(newvalue, xpifile, FALSE))
		{
			DWORD e = GetLastError();
			return TRUE;//*** Changed FALSE to TRUE
		}
	}

// If the string in script.ib matches "addrdfFile" perform this code
// This code decompresses the xpi files to which the rdf files must be 
// added and copies the rdf files (mailaccount.rdf and newsaccount.rdf) 
// from the Temp directory to the resulting directory after decompression
	else if (strcmp(cmdname, "addrdfFile") == 0)
	{
		char *xpiname	= strtok(NULL, ",)");	// xpi file name
		char *jname		= strtok(NULL, ",)");	// jar name within xpi file
		char *xpifile	= strtok(NULL, ",)");	// directory path within jar file
		char *value2    = strtok(NULL, ",)");	// name of rdf file to be added
		char *value 	= strtok(NULL, ",)");	// variable which specifies the path of the Temp dir 
		char *newvalue	= value;
		CString jarname = jname;

		if (value[0] == '%')
		{
			value++;
			char *t = strchr(value, '%');
			if (!t)
				return TRUE;//*** Changed FALSE to TRUE
			*t = '\0';
			newvalue = (char *)(LPCTSTR) GetGlobal(value);
		}

		if (!xpiname || !xpifile || !newvalue)
				return TRUE;//*** Changed FALSE to TRUE

		CString command;

		if (strcmp(value2,"newsaccount.rdf") != 0)
		{
			if (findXPI(xpiname, xpifile))
				return TRUE;
		}
		// nscpxpipath = \CCKTool\NSCPXPI
		CString xpiArchive = nscpxpiPath + "\\" + xpiname;
		// decompressing the directory path within the XPI file
		command = quotes +rootPath + "unzip.exe"+ quotes + "-o" + spaces + quotes + xpiArchive  + quotes + spaces + quotes + xpifile + "/*.*" + quotes;
		ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);

		CString xpifile1 = xpifile;
		CString tempval=value2;
		
		CString newxpifile = xpifile1 + "/" + tempval;
		// copy rdf file from Temp directory
		if (!CopyFile(newvalue, newxpifile, FALSE))
		{
			DWORD e = GetLastError();
			return TRUE;//*** Changed FALSE to TRUE
		}
	}

	else if ((strcmp(cmdname, "modifySidebar") == 0) || 
		(strcmp(cmdname, "modifyBookmarks") == 0) ||
		(strcmp(cmdname, "modifyHelpMenu") == 0))
	{
		// extract the file to be modified from the jar/xpi
		// and process the file
		
		char *xpiname   = strtok(NULL, ",)"); // xpi file name
		char *jarname   = strtok(NULL, ",)"); // jar name within xpi file
		char *filename  = strtok(NULL, ",)"); // file name within jar file

		CString inputFile = filename;
		inputFile.Replace("/","\\");
		inputFile = tempPath + "\\" + inputFile;
		CString outputFile = inputFile + ".new";				

		if (!xpiname || !filename)
			return TRUE;
               
		//check to see if it is a jar and then do accordingly
		if (stricmp(jarname,"no.jar")==0)
			ExtractXPIFile(xpiname, filename);
		else 
			ExtractJARFile(xpiname, (CString)jarname, filename);

		if (strcmp(cmdname, "modifySidebar") == 0)
			ModifySidebar(inputFile, outputFile);
		else if (strcmp(cmdname, "modifyBookmarks") == 0)
			ModifyBookmarks(inputFile, outputFile);
		else if (strcmp(cmdname, "modifyHelpMenu") == 0)
			ModifyHelpMenu(inputFile, outputFile);
			
		remove(inputFile);
		rename(outputFile, inputFile);
	}

	else if (strcmp(cmdname, "removeNIMRestart") == 0)
	{
		// extract the file to be modified from the jar/xpi
		// and process the file

		char *xpiname   = strtok(NULL, ",)"); // xpi file name
		char *jarname   = strtok(NULL, ",)"); // jar name within xpi file
		char *filename  = strtok(NULL, ",)"); // file name within jar file

		CString inputFile = filename;
		inputFile.Replace("/","\\");
		inputFile = tempPath + "\\" + inputFile;
		CString outputFile = inputFile + ".new";

		if (!xpiname || !filename)
        		return TRUE;

		//check to see if it is a jar and then do accordingly
		if (stricmp(jarname,"no.jar")==0)
  			ExtractXPIFile(xpiname, filename);
		else
      			ExtractJARFile(xpiname, (CString)jarname, filename);

		if(!(RemoveNIMRestart(inputFile, outputFile)))
			remove(outputFile);
		else 
		{	
			remove(inputFile);
			rename(outputFile, inputFile);
		}
	}
               
	else if ((strcmp(cmdname, "modifyDTD") == 0) ||
			(strcmp(cmdname, "modifyJS") == 0) ||
			(strcmp(cmdname, "modifyJS1") == 0) ||
			(strcmp(cmdname, "modifyJS2") == 0) ||
      (strcmp(cmdname, "modifyHomepageURL") == 0) ||
			(strcmp(cmdname, "modifyProperties") == 0) ||
			(strcmp(cmdname, "modifyHashedPrefString") == 0) ||
			(strcmp(cmdname, "modifyHashedPrefInt") == 0) ||
			(strcmp(cmdname, "modifyHashedPrefBool") == 0))
	{
		char *xpiname	= strtok(NULL, ",)");
		char *jname		= strtok(NULL, ",)");
		char *xpifile	= strtok(NULL, ",)");
		char *entity	= strtok(NULL, ",)");
		char *value 	= strtok(NULL, ",)");
		char *newvalue	= value;
		CString jarname = jname;

		if (value[0] == '%')
		{
			value++;
			char *t = strchr(value, '%');
			if (!t)
				return TRUE;//*** Changed FALSE to TRUE
			*t = '\0';
			newvalue = (char *)(LPCTSTR) GetGlobal(value);
		}


		if (!xpiname || !xpifile || !entity || !newvalue)
			return TRUE;//*** Changed FALSE to TRUE
		//check to see if it is a jar and then do accordingly
		if (jarname.CompareNoCase("no.jar")==0)
			ExtractXPIFile(xpiname, xpifile);
		else 
			ExtractJARFile(xpiname, jarname, xpifile);

		if (strcmp(cmdname, "modifyHashedPrefString") == 0)
			ModifyHashedPref(xpifile,entity,newvalue, "string", TRUE);
		else if (strcmp(cmdname, "modifyHashedPrefInt") == 0)
			ModifyHashedPref(xpifile,entity,newvalue, "int", TRUE);
		else if (strcmp(cmdname, "modifyHashedPrefBool") == 0)
			ModifyHashedPref(xpifile,entity,newvalue, "bool", TRUE);
    else if (strcmp(cmdname, "modifyHomepageURL") == 0)
      ModifyHomepageURL(xpifile,entity,newvalue);
		else if (strcmp(cmdname, "modifyJS") == 0)
			ModifyJS(xpifile,entity,newvalue, FALSE);
		else if (strcmp(cmdname, "modifyProperties") == 0)
			ModifyProperties(xpifile,entity,newvalue);
		else if (strcmp(cmdname, "modifyJS1") == 0)
			ModifyJS1(xpifile,entity,newvalue, FALSE);
		else if (strcmp(cmdname, "modifyJS2") == 0)
			ModifyJS2(xpifile,entity,newvalue, FALSE);
		else
		{
			// If the browser window's title bar text field is empty, 
			// the default browser value is displayed
			if ((strcmp(entity,"mainWindow.titlemodifier") == 0) &&
			(strcmp(newvalue,"") == 0))
				newvalue = "&brandShortName;";
			ModifyDTD(xpifile, entity, newvalue);
		}
	}
	else if (strcmp(cmdname, "wrapXPI") == 0)
	{
	}
	else if (strcmp(cmdname, "processMailNews") == 0)
	{
		ProcessMailNews();
	}
  else if (strcmp(cmdname, "processPrefsTree") == 0)
  {

	  char *prefsTreeFile	= strtok(NULL, ",)");
    CString fileWithPath = configPath + "\\" + prefsTreeFile;

    char *installFile = strtok(NULL, ",)");
    gstrInstallFile = installFile;

    char *cfgPrefFile = strtok(NULL, ",)");
    gstrCFGPrefFile = cfgPrefFile;

    char *plainPrefFile = strtok(NULL, ",)");
    gstrPlainPrefFile = plainPrefFile;

    ProcessPrefsTree(fileWithPath);
  }
  else if (strcmp(cmdname, "configureRemoteAdmin") == 0)
  {
    char *vConvert = strtok(NULL, ",)");  // if set, then do the convert to remote admin
		if (vConvert[0] == '%')
		{
			vConvert++;
			char *t = strchr(vConvert, '%');
			if (!t)
				return TRUE;
			*t = '\0';
			char *bConvert = (char *)(LPCTSTR) GetGlobal(vConvert);

      // The convert checkbox was not checked. No need to continue.
      if (strcmp(bConvert, "1") != 0)
        return TRUE;
		}
    
    char *url = strtok(NULL, ",)");
		if (url[0] == '%')
		{
			url++;
			char *t = strchr(url, '%');
			if (!t)
				return TRUE;
			*t = '\0';
			url = (char *)(LPCTSTR) GetGlobal(url);
      if (!url)
        return TRUE;
		}

    char *prefFile = strtok(NULL, ",)");
    
    if (!prefFile)
      return TRUE;

    ConfigureRemoteAdmin(url, prefFile);

  }
  else if (strcmp(cmdname, "modifyUserJS") == 0)
  {
    // modifyUserJS(XPIname, fileWithinXPI, JSsourcefile)      // within XPI
    // modifyUserJS(browser.xpi, bin, jsedit.jsc)              //    example

    // modifyUserJS(none, pathAndFilename, JSsourcefile)       // normal file
    // modifyUserJS(none, \autoadmin\test.jsc, jsedit.jsc)     //    example

		char *xpiname	 = strtok(NULL, ",)");
		char *filename = strtok(NULL, ",)");
		char *jssource = strtok(NULL, ",)");

    CString jsSourceFile = configPath + "\\" + jssource;

		// pull the cfg file out of the XPI
    //
    ExtractXPIFile(xpiname, filename);

    // replace the appropriate block of javascript
    //
    ModifyUserJS(filename, jsSourceFile);   

    // cfg file gets repackaged with call to ReplaceXPIFiles in StartIB after all the interpret calls,
    // so no need to repacked it ourself.

  }

  else if (strcmp(cmdname, "markcfgversion") == 0)
  {
		char *xpiname	 = strtok(NULL, ",)");
		char *filename = strtok(NULL, ",)");
		char *versiontxt = strtok(NULL, ",)");

    // make a substitution if the text is a variable
		if (versiontxt[0] == '%')
		{
			versiontxt++;
			char *t = strchr(versiontxt, '%');
			if (t)
      {
  			*t = '\0';
			  versiontxt = (char *)(LPCTSTR) GetGlobal(versiontxt);
      }
		}


		// pull the cfg file out of the XPI
    //
    ExtractXPIFile(xpiname, filename);

    // replace the appropriate block of javascript
    //
    MarkCFGVersion(filename, versiontxt);   

    // cfg file gets repackaged with call to ReplaceXPIFiles in StartIB after all the interpret calls,
    // so no need to repacked it ourself.

  }

  else
		return FALSE;//*** We have to handle this condition better.

	return TRUE;
}

void init_components()
{
	int i;
	WIDGET *w = findWidget("SelectedComponents");
	BuildComponentList(Components, &numComponents, iniSrcPath,0);
	// Turn off components that aren't selected
	for (i=0; i<numComponents; i++)
	{
		if ((strstr(ConvertUTF8toANSI(w->value), Components[i].name) == NULL))
		{
			if (!(Components[i].selected && Components[i].invisible))
			Components[i].selected = FALSE;
		}
		else 
			if ((Components[i].additional) || (Components[i].visible))
			Components[i].selected = TRUE;
	}
}
/*Post Beta - we will use the DISABLED key.
Now this is implemented the round about way here.
We have to take only the components that are chosen and mark the rest as disabled
Disabled doesn't work now - so what we are doing is rewriting every key in the sections.
besides that we are also deleting the keys in the setup types 2&3 so that we have only two 
as per request of mktg.
*/
void invisible()
{

	CString Setup0Long = "Program will be installed with the most common options";

	CString Setup1Long = "You may choose the options you want to install.  Recommended for advanced users.";

	WritePrivateProfileString("Setup Type0", NULL, "", iniDstPath);
	WritePrivateProfileString("Setup Type2", NULL, "", iniDstPath);

	WritePrivateProfileString("Setup Type0","Description Short",(LPCTSTR)Setup0Short,iniDstPath);
	WritePrivateProfileString("Setup Type0","Description Long", (LPCTSTR)Setup0Long,iniDstPath);
	WritePrivateProfileString("Setup Type2","Description Short",(LPCTSTR)Setup1Short,iniDstPath);
	WritePrivateProfileString("Setup Type2","Description Long", (LPCTSTR)Setup1Long,iniDstPath);
	WritePrivateProfileString("Setup Type1",NULL," ",iniDstPath);
	WritePrivateProfileString("Setup Type3",NULL," ",iniDstPath);
	CString Cee;
	tempWidget = findWidget("SelectedComponents");
	CString component;

	for (int i=0; i<numComponents; i++)
	{
		if (Components[i].selected)
		{
			component = Components[i].compname;	
			Cee.Format("C%d", componentOrder);
			WritePrivateProfileString("Setup Type0",(LPCTSTR)Cee,(LPCTSTR)component, iniDstPath);
			WritePrivateProfileString("Setup Type2",(LPCTSTR)Cee,(LPCTSTR)component, iniDstPath);

			CString strAttributes = "SELECTED";
			CString strSep = "|";

			if (Components[i].disabled)
				strAttributes += strSep + "DISABLED";

			if (Components[i].invisible)
				strAttributes += strSep + "INVISIBLE";

			if (Components[i].visible)
				strAttributes += strSep + "VISIBLE";

			if (Components[i].uncompress)
				strAttributes += strSep + "UNCOMPRESS";

			if (Components[i].launchapp)
				strAttributes += strSep + "LAUNCHAPP";

			if (Components[i].additional)
				strAttributes += strSep + "ADDITIONAL";

			if (Components[i].forceupgrade)
				strAttributes += strSep + "FORCE_UPGRADE";

			if (Components[i].supersede)
				strAttributes += strSep + "SUPERSEDE";

			if (Components[i].downloadonly)
				strAttributes += strSep + "DOWNLOAD_ONLY";

			if (Components[i].ignoreerror)
				strAttributes += strSep + "IGNORE_DOWNLOAD_ERROR";

			if (Components[i].unselected)
			{
				// UNSELECTED attribute must not appear with SELECTED attribute
				if ( (strAttributes.Find("SELECTED")) != -1 )
					strAttributes.Replace("SELECTED", "UNSELECTED");
			}
			
			if (strcmp(strAttributes, "SELECTED") == 0)
			// Include INVISIBLE attribute for AOD component
				WritePrivateProfileString(Components[i].compname, "Attributes", 
				"SELECTED|INVISIBLE", iniDstPath);
			else
				WritePrivateProfileString(Components[i].compname, "Attributes", 
				strAttributes, iniDstPath);
			componentOrder++;
		}
		else
			WritePrivateProfileString(Components[i].compname, "Attributes",	
			"INVISIBLE", iniDstPath);
	}
}

void LinuxInvisible()
{
	Setup0Short = "Recommended";
	Setup1Short = "Custom";
	CString Setup0Long = "Installs the most common options.  Quickest to download; recommended for most users.  Java not included.";
	CString Setup1Long = "Recommended for advanced users or users with specific international language package requirements.  (Navigator will be installed by default.)";

	WritePrivateProfileString("Setup Type0", NULL, "", iniDstPath);
	WritePrivateProfileString("Setup Type1", NULL, "", iniDstPath);

	WritePrivateProfileString("Setup Type0","Description Short",
		(LPCTSTR)Setup0Short,iniDstPath);
	WritePrivateProfileString("Setup Type0","Description Long", 
		(LPCTSTR)Setup0Long,iniDstPath);
	WritePrivateProfileString("Setup Type1","Description Short",
		(LPCTSTR)Setup1Short,iniDstPath);
	WritePrivateProfileString("Setup Type1","Description Long", 
		(LPCTSTR)Setup1Long,iniDstPath);
	WritePrivateProfileString("Setup Type2",NULL," ",iniDstPath);
	WritePrivateProfileString("Setup Type3",NULL," ",iniDstPath);
	CString Cee;
	tempWidget = findWidget("SelectedComponents");
	CString component;
	for (int i=0; i<numComponents; i++)
	{
		if (Components[i].selected)
		{
			component = Components[i].compname;	
			Cee.Format("C%d", componentOrder);
			WritePrivateProfileString("Setup Type0",(LPCTSTR)Cee,
				(LPCTSTR)component,iniDstPath);
			WritePrivateProfileString("Setup Type1",(LPCTSTR)Cee,
				(LPCTSTR)component,iniDstPath);

			CString strAttributes = "SELECTED";
			CString strSep = "|";

			if (Components[i].invisible)
				strAttributes += strSep + "INVISIBLE";

			if (Components[i].downloadonly)
				strAttributes += strSep + "DOWNLOAD_ONLY";

			if (Components[i].visible)
			// VISIBLE attribute must appear by itself 
			// this may require change in future if VISIBLE attribute in the 
			// browser installer appears with other attributes
				strAttributes = "VISIBLE";

			WritePrivateProfileString(Components[i].compname, "Attributes",
				strAttributes, iniDstPath);
			componentOrder++;
		}
		else
			WritePrivateProfileString(Components[i].compname,"Attributes",
			"INVISIBLE",iniDstPath);
	}
}

void AddThirdParty()
{
	CString tpCompPath1 = GetGlobal("CustomComponentPath1");
	CString tpCompPath2 = GetGlobal("CustomComponentPath2");
	CString tpComp1		= ConvertUTF8toANSI(GetGlobal("CustomComponentDesc1"));
	CString tpComp2		= ConvertUTF8toANSI(GetGlobal("CustomComponentDesc2"));
	CString tpCompSize1	= GetGlobal("CustomComponentSize1");
	CString tpCompSize2	= GetGlobal("CustomComponentSize2");
	CString componentName;
	CString cName;
	CString compSDesc	= "Description Short=";
	CString compLDesc	= "Description Long=";
	CString compArchive = "Archive=";
	CString compISize	= "Install Size Archive=";
	CString compAttrib	= "Attributes=SELECTED|LAUNCHAPP";
	int archiveLen		= tpCompPath1.GetLength();
	int findLen			= tpCompPath1.ReverseFind('\\');
	CString Archive1	= tpCompPath1.Right(archiveLen - findLen -1);
	archiveLen			= tpCompPath2.GetLength();
	findLen				= tpCompPath2.ReverseFind('\\');
	CString Archive2	= tpCompPath2.Right(archiveLen - findLen -1);
	CString tempstr;
	char *cBuffer1 = new char [MAX_SIZE];
	char *p = cBuffer1;
	strcpy(p,LPCTSTR(compSDesc + tpComp1));
	tempstr = compSDesc + tpComp1;
	p += (tempstr.GetLength() +1);
	strcpy(p,LPCTSTR(compLDesc + tpComp1));
	tempstr = compLDesc + tpComp1;
	p += (tempstr.GetLength() +1);
	strcpy(p,LPCTSTR(compArchive + Archive1));
	tempstr = compArchive + Archive1;
	p += (tempstr.GetLength() +1);
	strcpy(p,LPCTSTR(compISize + tpCompSize1));
	tempstr = compISize + tpCompSize1;
	p += (tempstr.GetLength() +1);
	strcpy(p,LPCTSTR(compAttrib));
	p += (compAttrib.GetLength() +1);
	*p = 0;
	char *cBuffer2 = new char [MAX_SIZE];
	char *q = cBuffer2;
	strcpy(q,LPCTSTR(compSDesc + tpComp2));
	tempstr = compSDesc + tpComp2;
	q += (tempstr.GetLength() +1);
	strcpy(q,LPCTSTR(compLDesc + tpComp2));
	tempstr = compLDesc + tpComp2;
	q += (tempstr.GetLength() +1);
	strcpy(q,LPCTSTR(compArchive + Archive2));
	tempstr = compArchive + Archive2;
	q += (tempstr.GetLength() +1);
	strcpy(q,LPCTSTR(compISize + tpCompSize2));
	tempstr = compISize + tpCompSize2;
	q += (tempstr.GetLength() +1);
	strcpy(q,LPCTSTR(compAttrib));
	q += (compAttrib.GetLength() +1);
	*q = 0;


/*
	char cBuffer1[MAX_SIZE][MAX_SIZE]={LPCTSTR(compSDesc + tpComp1),LPCTSTR(compLDesc + tpComp1),
					 LPCTSTR(compArchive + Archive1),LPCTSTR(compISize + tpCompSize1),
					 LPCTSTR(compAttrib)};
	char *cBuffer2[]={LPCTSTR(compSDesc + tpComp2),LPCTSTR(compLDesc + tpComp2),
					 LPCTSTR(compArchive + Archive2),LPCTSTR(compISize + tpCompSize2),
					 LPCTSTR(compAttrib)};
*/
	
	CString firstSix = tpCompPath1.Left(6);

	if ((firstSix.CompareNoCase("Please") != 0) && !(tpCompPath1.IsEmpty()))
	{
		componentName.Format("C%d", (numComponents));
		cName.Format("C%d", componentOrder);
		componentOrder++;

		WritePrivateProfileString("Setup Type0", cName, componentName, iniDstPath);
		WritePrivateProfileString("Setup Type2", cName, componentName, iniDstPath);
		WritePrivateProfileSection(componentName, cBuffer1, iniDstPath);
		numComponents++;
		CopyFile(tpCompPath1, xpiDstPath + "\\" + Archive1, FALSE);
		DWORD e1 = GetLastError();

	}

	firstSix = tpCompPath2.Left(6);
	if ((firstSix.CompareNoCase("Please") != 0) && !(tpCompPath2.IsEmpty()))
	{
		componentName.Format("C%d", (numComponents));
		cName.Format("C%d", componentOrder);

		WritePrivateProfileString("Setup Type0", cName, componentName, iniDstPath);
		WritePrivateProfileString("Setup Type2", cName, componentName, iniDstPath);
		WritePrivateProfileSection(componentName, cBuffer2, iniDstPath);
		CopyFile(tpCompPath2, xpiDstPath + "\\" + Archive2, FALSE);
		DWORD e2 = GetLastError();
	}
	delete [] cBuffer1;
	delete [] cBuffer2;
}
	HRESULT CreateShortcut(const CString Target, const CString Arguments, const CString
                     LinkFileName, const CString WorkingDir, bool IsFolder)
 {
         // Initialize OLE libraries
         if (!AfxOleInit())
                 return FALSE;
         
         HRESULT hres;
         
         CString Desktop=getenv("USERPROFILE");
         Desktop += "\\Desktop\\";
         CString Link = Desktop + LinkFileName;
         
         if (!IsFolder)
         {
                 IShellLink* psl;
                 hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink,
 (LPVOID*) &psl);
                 if (SUCCEEDED(hres))
                 {
                         IPersistFile* ppf;
                         
                         psl->SetPath(Target);
                         psl->SetArguments(Arguments);
                         psl->SetWorkingDirectory(WorkingDir);
                         
                         hres = psl->QueryInterface( IID_IPersistFile, (LPVOID *) &ppf);
                         
                         if (SUCCEEDED(hres))
                         {
                                 CString Temp = Link;
                                 Temp.MakeLower();
                                 if (Temp.Find(".lnk")==-1)
                                         Link += ".lnk";  // Important !!!
                                 WORD wsz[MAX_PATH];
                                 MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, Link, -1, wsz,MAX_PATH);
                                 
                                 hres = ppf->Save(wsz, TRUE);
                                 
                                 ppf->Release();
                         }
                         psl->Release();
                 }
         }
         else
         {
                 hres = _mkdir(Link);
         }
         return hres;
 }
BOOL GetRegistryKey( HKEY key, char *subkey, char *retdata )
{ 
	long retval;
	HKEY hkey;

	retval = RegOpenKeyEx(key, subkey, 0, KEY_QUERY_VALUE, &hkey);

	if(retval == ERROR_SUCCESS)
	{
		long datasize = MAX_SIZE;
		char data[MAX_SIZE];

		RegQueryValue(hkey,NULL,(LPSTR)data,&datasize);

		lstrcpy(retdata,data);
		RegCloseKey(hkey);
	}
	return retval;
}

CString GetBrowser(void)
{
	char key[MAX_SIZE + MAX_SIZE];
	CString retflag = "";

	/* get the .htm regkey and lookup the program */
	if(GetRegistryKey(HKEY_CLASSES_ROOT,".htm",key) == ERROR_SUCCESS)
	{
		lstrcat(key,"\\shell\\open\\command");
		if(GetRegistryKey(HKEY_CLASSES_ROOT,key,key) == ERROR_SUCCESS)
		{
			char *pos;
			pos = strstr(key,"\"%1\"");
			if(pos == NULL)     /* if no quotes */
			{
				/* now check for %1, without the quotes */
				pos = strstr(key,"%1");
				if(pos == NULL) /* if no parameter */
				pos = key+lstrlen(key)-1;
				else
				*pos = '\0';    /* remove the parameter */
			}
			else
			*pos = '\0';        /* remove the parameter */
			retflag = key;
		}
		
	}

	return retflag;
}

CString SubstituteValues(CString inLine)
// Replace tokens in script template with appropriate locale values
{
	CString newValue, oldValue, strTemp;
	int tokenStartPos, tokenEndPos,
		searchPos=0, count=0;
	int inLineLength = inLine.GetLength();
	while (count < inLineLength)
	{
		tokenStartPos = inLine.Find('%', searchPos);
		if (tokenStartPos != -1)
		{
			tokenEndPos = inLine.Find('%', tokenStartPos+1);
			tokenEndPos += 1;
			strTemp = inLine.Left(tokenEndPos);
			oldValue = strTemp.Right(tokenEndPos-tokenStartPos);

			WIDGET *w = findWidget(oldValue);
			if (w)
			{
				 newValue = GetGlobal(oldValue);
				 searchPos = 0;
			}
			else
			{
				newValue = oldValue;
				searchPos = tokenEndPos;
			}

			inLine.Replace(oldValue, newValue);
			count = tokenEndPos;
		}
		else
			count = inLineLength;
	}
	return inLine;
}

void CreateLinuxInstaller()
{
	char currentdir[_MAX_PATH];
	_getcwd(currentdir,_MAX_PATH);
	
	// delete tar.gz file of output dir if it already exists
	if (FileExists(xpiDstPath+"\\"+targzfile))
		DeleteFile(xpiDstPath+"\\"+targzfile);
	
	// Copy customized files to templinux directory
	CopyDirectory(xpiDstPath, templinuxPath + xpiDir, TRUE);
	CopyFile(xpiDstPath+"\\Config.ini", templinuxPath+"\\Config.ini",FALSE);
	DeleteFile(templinuxPath + xpiDir + "\\Config.ini");

	// Remove extra carriage returns in config.ini file
	FILE *fout = fopen(templinuxPath+"\\config.tmp", "wb");
	if (!fout)
	{
		AfxMessageBox("Cannot open output file", MB_OK);
		exit(3);
	}	
	FILE *fin = fopen(templinuxPath+"\\Config.ini", "rb");
	if (!fin)
	{
		AfxMessageBox("Cannot open Config.ini file", MB_OK);
		exit(3);
	}
	else 
	{
		char inbuf[BUF_SIZE], outbuf[BUF_SIZE];
		int cnt2=0;
		while(!feof(fin))
		{
			int count = fread(&inbuf, sizeof(char), sizeof(inbuf), fin);
			if (ferror(fin))
			{
				AfxMessageBox("Error in reading Config.ini file", MB_OK);
				exit(3);
			}
			char *cpin = inbuf;
			char *cpout = outbuf;
			while (count-- > 0)
			{
				if (*cpin == CRVALUE)
					cpin++;
				else
				{
					*cpout++ = *cpin++;
					cnt2++;
				}
			}
			fwrite(&outbuf, sizeof(char), cnt2, fout);
			if (ferror(fout))
			{
				AfxMessageBox("Error in writing Config.ini file", MB_OK);
				exit(3);
			}
			cnt2=0;
		}
		fclose(fin);
	}
	fputs("[END]\n", fout);
	fclose(fout);
	DeleteFile(templinuxPath+"\\Config.ini");
	rename(templinuxPath+"\\config.tmp", templinuxPath+"\\config.ini");	

	_chdir(outputPath);
	int pos = targzfile.ReverseFind('.');
	CString tarfile = targzfile.Left(pos);
	CopyFile(nscpxpiPath+"\\"+tarfile, xpiDstPath+"\\"+tarfile, FALSE);
	templinuxDirPath.Replace("\\", "/");
	templinuxDirPath.Replace(":","");
	templinuxDirPath.Insert(0,"/cygdrive/");
	
	// Add customized xpi files to linux tar file
	CString command = "tar -rvf " + tarfile + " -C " + quotes + 
		templinuxDirPath + quotes + spaces + strNscpInstaller;
	ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);

	// Compress tar file to create customized tar.gz file
	command = "gzip " + tarfile;
	ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
	_chdir(currentdir);
}

void CreateMacZipFile()
// Creating a single customized Mac zip file which contains mac scripts
// and customized mac files
{
	CString customizedZipFile = "CustomizedMacFiles.zip";
	if (FileExists(customizedZipFile))
		DeleteFile(customizedZipFile);
	// Adding Mac scripts shipped with the tool to the final Mac zip file
	CString command = quotes + rootPath + "zip.exe" + quotes + spaces + "-j" + 
		spaces + quotes + xpiDstPath + "\\" + customizedZipFile + quotes + 
		spaces + quotes + platformPath + "\\*.*" + quotes;
	ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
	// Adding customized mac files created in output directory to the 
	// final Mac zip file
	command = quotes + rootPath + "zip.exe" + quotes + spaces + "-jm" + 
		spaces + quotes + xpiDstPath + "\\" + customizedZipFile + quotes + 
		spaces + quotes + xpiDstPath + "\\*.*" + quotes + " -x autoconfig.jsc";
	ExecuteCommand((char *)(LPCTSTR) command, SW_HIDE, INFINITE);
}

void InsertComma(CString& requiredSpace)
{
	int len = requiredSpace.GetLength();
	int pos = len%3;
	if (pos == 0)
		pos = 3;
	for(int i=pos; i<len; i+=3)
	{
		requiredSpace.Insert(i,',');
		i++;
	}
}

void DiskSpaceAlert(ULONGLONG required, ULONGLONG available)
{
	char tempavailspace[20], tempreqspace[20];
	CString availableSpace, requiredSpace;

	_ui64toa(available, tempavailspace, 10);
	availableSpace = tempavailspace;
	InsertComma(availableSpace);

	_ui64toa(required, tempreqspace, 10);
	requiredSpace = tempreqspace;
	InsertComma(requiredSpace);

	AfxMessageBox("Not enough disk space. Required: "+requiredSpace+" bytes. Available: "+availableSpace+" bytes.", MB_OK);
}

ULONGLONG FindDirSize(CString dirPath)
// Find the total size of a directory in bytes 
{
	ULONGLONG dirSize = 0;
	CFileFind fileFind;

	dirPath += "\\*.*";
	BOOL bFound = fileFind.FindFile (dirPath);

	while (bFound)
	{
		bFound = fileFind.FindNextFile ();
		if (fileFind.IsDirectory () && !fileFind.IsDots ())
		{
			dirSize += FindDirSize (fileFind.GetFilePath ());
		}
		else
		{
			dirSize += fileFind.GetLength ();
		}
	}
	return dirSize;
}

ULONGLONG FindFileSize(CString fileName)
// Find the size of a file in bytes 
{
	CFile file(fileName, CFile::modeReadWrite);
	ULONGLONG fileSize = file.GetLength();
	file.Close();
	return fileSize;
}

ULONGLONG ComputeReqdWinDiskSpace()
/*
Compute required disk space for Windows build
Required disk space =
 Total size for selected components +
 Total size for third party components +
 Total size for CD autorun +  
 Total size used for various file operations +
 Size of skinclas.xpi +
 Total size of setupfiles (contents of NSSetup.zip) and NSSetup.exe 
*/
{
	ULONGLONG requiredSize=0;
	
	// Total size of selected components 
	for (int i=0; i<numComponents; i++)
	{
		if (Components[i].selected)
			requiredSize += FindFileSize(nscpxpiPath + "\\" + Components[i].archive);
	}

	// Total size of thirdparty components
	if ( !((GetGlobal("CustomComponentPath1")).IsEmpty()) )
		requiredSize += FindFileSize(GetGlobal("CustomComponentPath1"));
	if ( !((GetGlobal("CustomComponentPath2")).IsEmpty()) )
		requiredSize += FindFileSize(GetGlobal("CustomComponentPath2"));

	// Total size of CD autorun files
	CString cdDir= GetGlobal("CD image");
	if (cdDir.Compare("1")==0)
		requiredSize += FindDirSize(shellPath);

	// Approximate size (4MB) for various file operations
	requiredSize += 4194304;
	
	// size of skinclas.xpi since it gets copied even though deselected
	requiredSize += FindFileSize(nscpxpiPath + "\\skinclas.xpi");

	// size of setup files and NSSetup.exe
	requiredSize += 691926;

	return requiredSize;
}

ULONGLONG ComputeReqdMacDiskSpace()
/*
Compute required disk space for Mac OS build
Required disk space =
 Total size of Mac scripts and zip file +
 Total size for various file operations
*/
{
	ULONGLONG requiredSize=0;

	// size of Mac scripts and zip file
	requiredSize += FindDirSize(platformPath);

	// Approximate size (4MB) for various file operations
	requiredSize += 4194304;

	return requiredSize;
}

BOOL FillGlobalWidgetArray(CString file)
{
	char buffer[MAX_SIZE] = {'\0'};
	CString name = "";
	CString value = "";

       	FILE *globs = fopen(file, "r");
	if (!globs)
		return FALSE;

	while(!feof(globs))
	{
		while(fgets(buffer, MAX_SIZE, globs))
		{
			if (strstr(buffer, "="))
			{
				name =  CString(strtok(buffer,"="));
				value = CString(strtok(NULL,"="));
				value.TrimRight();
				if (value.Find("%") >= 0) {
					//We should never enter this condition via wizardmachine.exe 
					AfxMessageBox("The current .che file called: "+file+" contains some unfilled parameters."
							"These parameters will appear between two percent (%) signs such as %Root%"
							"Please replace these parameters with their appropriate values and restart"
							"the program", MB_OK);
			//		value=theInterpreter->replaceVars((char *) (LPCSTR) value,NULL);
				}
				
				WIDGET* w = SetGlobal(name, value);
				if (w)
					w->cached = TRUE;
			}
		}
	}

	fclose(globs);

	return TRUE;
}



int StartIB(/*CString parms, WIDGET *curWidget*/)
{
	char *fgetsrv;
	int rv = TRUE;
	char	olddir[1024];
	CString curVersion, localePath,	strLang, strRegion, strREGION;

	componentOrder = 0;
	rootPath = GetModulePath();
	SetGlobal("Root", rootPath);
	configName = GetGlobal("_NewConfigName");
	SetGlobal("CustomizationList", configName); 

	curVersion      = GetGlobal("Version");
	curPlatform     = GetGlobal("lPlatform");
	platformPath    = rootPath+"Version\\"+curVersion+"\\"+curPlatform;
	curLanguage     = GetLocaleCode(GetGlobal("Language"));
	localePath      = rootPath+"Version\\"+curVersion+"\\"+curPlatform+"\\"+curLanguage;
	configPath      = rootPath + "Configs\\" + configName;
	outputPath      = configPath + "\\Output";
	cdPath          = configPath + "\\Output\\Core";
	cdshellPath     = configPath + "\\Output\\Shell";
	remoteAdminFile = outputPath + "\\autoconfig.jsc";
	networkPath     = configPath + "\\Network";
	tempPath        = configPath + "\\Temp";
	iniDstPath      = cdPath + "\\config.ini";
	scriptPath      = rootPath + "script_" + curPlatform + ".ib";
	workspacePath   = configPath + "\\Workspace";
	shellPath       = workspacePath + "\\Autorun\\";
	xpiDstPath      = cdPath;

	// variables for language-region information
	strLang        = curLanguage.Left(2);
	strRegion      = curLanguage.Right(2);
	strREGION      = strRegion;
	strREGION.MakeUpper();
	SetGlobal("%lang%", strLang);
	SetGlobal("%region%", strRegion);
	SetGlobal("%REGION%", strREGION);

	// initializing variables for CCK linux build
	templinuxPath    = tempPath + "\\templinux\\netscape-installer";
	xpiDir           = "\\xpi";
	templinuxDir     = "tempLinux";
	targzfile        = "netscape-i686-pc-linux-gnu-sea.tar.gz";
	templinuxDirPath = tempPath + "\\" + templinuxDir;

//  AfxMessageBox("set breakpoint",MB_OK);

	// delete contents of output directory before creating customized installer
	EraseDirectory(outputPath);

	if (SearchPath(workspacePath, "NSCPXPI", NULL, 0, NULL, NULL))
		nscpxpiPath = workspacePath + "\\NSCPXPI";
	else
		nscpxpiPath = localePath + "\\Nscpxpi";

	if (curPlatform == "Linux")
	{
		// Create directories for temporarily storing customized files
		_mkdir(tempPath);
		_chdir(tempPath);
		_mkdir(templinuxDir);
		_mkdir(templinuxPath);
		_mkdir(templinuxPath + xpiDir);
	}

	iniSrcPath	= nscpxpiPath + "\\config.ini";
	init_components();

//Check for disk space before continuing
	ULARGE_INTEGER nTotalBytes, nTotalFreeBytes, nTotalAvailable;
	GetDiskFreeSpaceEx(NULL,&nTotalAvailable, &nTotalBytes, &nTotalFreeBytes);
	if (curPlatform == "Windows")
	{
		ULONGLONG reqdWinDiskSpace = ComputeReqdWinDiskSpace();
		if ((nTotalAvailable.QuadPart) < reqdWinDiskSpace)
		{
			DiskSpaceAlert(reqdWinDiskSpace, (nTotalAvailable.QuadPart));
			return FALSE;
		}
	}
	else if (curPlatform == "Linux")
	{
		if ((nTotalAvailable.QuadPart) < LDISK_SPACE)
		{
			DiskSpaceAlert(LDISK_SPACE,(nTotalAvailable.QuadPart));
			return FALSE;
		}
	}
	else if (curPlatform == "Mac OS")
	{
		ULONGLONG reqdMacDiskSpace = ComputeReqdMacDiskSpace();
		if ((nTotalAvailable.QuadPart) < reqdMacDiskSpace)
		{
			DiskSpaceAlert(reqdMacDiskSpace, (nTotalAvailable.QuadPart));
			return FALSE;
		}
	}
//Check for Disk space over

	
//checking for the autorun CD shell - inorder to create a Core dir or not
	CString cdDir= GetGlobal("CD image");
	CString networkDir = GetGlobal("Network");
	CString ftpLocation = GetGlobal("FTPLocation");
//checking to see if the AnimatedLogoURL has a http:// appended in front of it 
//if not then we have to append it;

//Check to see if the User Agent string exists and if so then prefix with -CK
//(-CK can be replaced with a string UserAgentPrefix global.)
	CString userAgentPrefix = GetGlobal("UserAgentPrefix");
	if (userAgentPrefix.IsEmpty())
		userAgentPrefix = "CK-";
	CString userAgent = GetGlobal("OrganizationName");
    if (userAgent)
    {
        CString templeft = userAgent.Left(3);
        if ((templeft.CompareNoCase(userAgentPrefix)) != 0)
            userAgent = userAgentPrefix + userAgent;
    }
    SetGlobal("OrganizationName",userAgent);


// check to see if the bmp for rshell background is bigger than 302KB;
	HANDLE hFile;
	DWORD dwFileSize;
	CString Rshellbmp = GetGlobal("ShellBgBitmap");

	hFile = CreateFile ((LPCTSTR)Rshellbmp,GENERIC_READ,FILE_SHARE_READ,NULL,
							OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,(HANDLE)NULL);

	dwFileSize = GetFileSize (hFile, NULL);
	int returnValue;
	if (dwFileSize < 300000)
		returnValue = AfxMessageBox("You have chosen a background BitMap that is too small for the customized RShell-If you want to proceed anyway choose Cancel or click on Retry and go back and make your changes",MB_RETRYCANCEL);
	if (returnValue == IDRETRY)
		return FALSE;

//end of filesize check;

// Check to see if the animatedlogourl has the http before it or not- else the browser
// looks only in the chrome directory.
	CString animLogoUrl = GetGlobal("AnimatedLogoURL");
	CString leftvalue = animLogoUrl.Left(7);
	CString httpvalue = "http://";
	if (leftvalue.CompareNoCase("http://") != 0)
	{
		httpvalue = httpvalue + animLogoUrl;
		SetGlobal("AnimatedLogoURL",httpvalue);
	}
	
	CString HelpUrl = GetGlobal("HelpMenuCommandURL");

	leftvalue = HelpUrl.Left(7);
	httpvalue = "http://";
	if (leftvalue.CompareNoCase("http://") != 0)
	{
		httpvalue = httpvalue + HelpUrl;
		SetGlobal("HelpMenuCommandURL",httpvalue);
	}

	CString setIspRDF = tempPath +"\\mailaccount.rdf";
	SetGlobal("IspRDF",setIspRDF);
	CreateIspMenu();

	CString setnewsRDF = tempPath +"\\newsaccount.rdf";
	SetGlobal("NewsRDF",setnewsRDF);
	CreateNewsMenu();

	// Determine which proxy configuration is chosen
	CString proxyConfigoption = GetGlobal("radioGroup2");
	if (proxyConfigoption == "3")
		SetGlobal("ProxyConfig","2");
	else if (proxyConfigoption == "2")
		SetGlobal("ProxyConfig","1");
	else
		SetGlobal("ProxyConfig","0");
	// Determine which SOCKS version is chosen
	CString socksVer = GetGlobal("socksv");
	if (socksVer == "SOCKS v4")
		SetGlobal("SocksVersion","4");
	else
		SetGlobal("SocksVersion","5");

	if ((cdDir.Compare("1")==0) && (curPlatform.Compare("Windows")==0))
	{
		_mkdir((char *)(LPCTSTR) cdPath);
	}
	else
	{
		iniDstPath = outputPath + "\\config.ini";
		xpiDstPath = outputPath;
	}
/////////////////////////////
	_mkdir((char *)(LPCTSTR) tempPath);
	_mkdir((char *)(LPCTSTR) workspacePath);
	// Copying config.ini file to the output directory
	if (!CopyFile(iniSrcPath, iniDstPath, TRUE))
		DWORD e = GetLastError();
//	_mkdir((char *)(LPCTSTR) cdshellPath);
	GetCurrentDirectory(sizeof(olddir), olddir);

	if(SetCurrentDirectory((char *)(LPCTSTR) tempPath) == FALSE)
	{
		AfxMessageBox("Windows System Error:Unable to change directory",MB_OK);
		return TRUE;
	}
//PostBeta - We have to inform the user that he has not set any value
//and that it will default. Returning TRUE so that it doesn't stay in the last
//screen forever.  

	// Read in script file and interpret commands from it
	FILE *f = fopen(scriptPath, "r");
	if (!f)
		rv = FALSE;
	else
	{
		int done = FALSE;
		while (!done)
		{
			fgetsrv = fgets(buffer, sizeof(buffer), f);
			done = feof(f);
			if (!done)
			{
				if (!fgetsrv || ferror(f))
				{
					rv = FALSE;
					break;
				}

				buffer[strlen(buffer)] = '\0';  // Eliminate the trailing newline
				CString strTempBuffer = buffer;
				strcpy(buffer, SubstituteValues(strTempBuffer));

				if (!interpret(buffer))
				{
					rv = FALSE;
					break;
				}
			}
		}

		fclose(f);
	}


  // Copy the .cfg, if it exists, to the output directory in case the user
  // wants to save it or look at it or something.
  if (!gstrCFGPrefFile.IsEmpty())
  {
    int ipos = gstrCFGPrefFile.ReverseFind('/');
    if (ipos >= 0)
    {
      CString baseFilename = gstrCFGPrefFile.Right(gstrCFGPrefFile.GetLength() - (ipos + 1)); 
      CopyFile(tempPath+"\\bin\\"+baseFilename, outputPath+"\\"+baseFilename, FALSE);  // FALSE to overwrite
    }
  }

	// Put all the extracted files back into their new XPI homes
	ReplaceJARFiles();


	ReplaceXPIFiles();


	// Copy remaining default installer files into config
	// preserving any existing files that we created already
	// in previous steps
	/* -- Need to be more selective than this
	CopyDir(nscpxpiPath, cdPath, NULL, FALSE);
	*/

	for (int i=0; i<numComponents; i++)
	{
		if (Components[i].selected)
			CopyFile(nscpxpiPath + "\\" + Components[i].archive, 
					 xpiDstPath + "\\" + Components[i].archive, TRUE);
	}

	if (curPlatform == "Windows")
	{
		if (cdDir.Compare("1") ==0)
		{
			CopyDir(shellPath, outputPath, NULL, TRUE);
			CreateRshell ();
			WritePrivateProfileString("Message Stream", "Status", "Disabled", iniDstPath);
		}
		else
		{
			FILE *infout;
			CString infFile = outputPath + "\\autorun.inf";
			infout = fopen(infFile, "w");
			if (!infout)
				exit( 3 );
			fprintf(infout,"[autorun]\nopen = setup.exe");
		}

		CString component;
		CString configiniPath = xpiDstPath +"\\config.ini";

		if (ftpLocation.Compare("ftp://") !=0)
		{
//			Change the ftp section to accomodate changes from PR3 to RTM
//			for (int i=0; i<numComponents; i++)
//			{
//				if (Components[i].selected)
//					CopyFile(nscpxpiPath + "\\" + Components[i].archive, 
//						networkPath + "\\" + Components[i].archive, TRUE);

			WritePrivateProfileString("General", "url", ftpLocation, configiniPath);
			WritePrivateProfileString("Site Selector", "Domain0", ftpLocation, configiniPath);
			// HTTP support for network installer 
			CString httpstr = ftpLocation.Left(7);
			if (httpstr.Compare("http://") == 0)
				WritePrivateProfileString("Dialog Advanced Settings",
				"Use Protocol", "HTTP", configiniPath);
			else
				WritePrivateProfileString("Dialog Advanced Settings",
				"Use Protocol", "FTP", configiniPath);
//			}
		}
		
		invisible();
		AddThirdParty();
		ReplaceINIFile();
	}

	else if (curPlatform == "Linux")
	{
		LinuxInvisible();
		AddThirdParty();
		CreateLinuxInstaller();
	}

	else if (curPlatform == "Mac OS")
	{
		CreateMacZipFile();
	}

	SetCurrentDirectory(olddir);
	CString TargetDir = GetGlobal("Root");
	CString TargetFile = TargetDir + "wizardmachine.ini";
	CString MozBrowser = GetBrowser();
//	CreateShortcut(MozBrowser, TargetFile, "HelpLink", TargetDir, FALSE);

	SetCurrentDirectory(configPath);
	EraseDirectory(tempPath);
	RemoveDirectory(tempPath);

	return TRUE;

}


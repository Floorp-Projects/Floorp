#include "stdafx.h"
#include <Winbase.h>
#include <direct.h>
#include <stdio.h>
#include "ib.h"
#include "globals.h"
#include "fstream.h"
#include <afxtempl.h>
#include <afxdisp.h>
#include "resource.h"
#include "NewDialog.h"

int main(int argc, char *argv[])
{
  CString configPath;
  CString rootPath;
  CString templateDir;
  CString configname;
  CString configpath;
  CString che_path;
  CString che_file;
  
  if(argc == 3)
  {
    configPath = argv[2];
    FillGlobalWidgetArray(configPath);

    if(!strcmp(argv[1], "-c"))
    {
      //The option "-c" means that ibengine.exe 
      //was called from wizardmachine.exe

      StartIB();
    }
    else if(!strcmp(argv[1], "-u"))
    {
      //The option "-u" means that the user is 
      //running ibengine.exe via command line

      rootPath = GetModulePath();
      templateDir = rootPath + "WSTemplate";
      configname = GetGlobal("_NewConfigName");
      configpath = rootPath + "Configs\\" + configname;

      //Grab exact name of the .che file from configPath
      che_file = configPath;
      int extractposition = che_file.ReverseFind('\\');
      extractposition++;
      extractposition = (che_file.GetLength()) - extractposition;
      che_file = che_file.Right(extractposition);

      //These are some commands that we only want to run if 
      //ibengine.exe is run at command line

      //Create the config path
      _mkdir(configpath);

      //Copy files and directories from WSTemplate 
      //to new config directory
      CopyDir(templateDir, configpath, NULL, FALSE);

      //Copy the .che file given on command line to the 
      //appropriate directory
      che_path = configpath + "\\" + che_file;
      CopyFile(configPath, che_path, FALSE);

      StartIB();

      printf("\nInstaller creation is complete.  The build is "
        "in %sConfigs\\%s\\Output\n", rootPath, configname); 

    }
    else
    {
      printf("\nYou have supplied incorrect command line options. \n"
             "Please, run either \"WizardMachine.exe\" "
             "or \"ibengine.exe -u <your_config_file_path>\"\n");
      return 1;
    } 
  }
  else
  {
    printf("\nYou have supplied incorrect command line options. \n"
           "Please, run either \"WizardMachine.exe\" "
           "or \"ibengine.exe -u <your_config_file_path>\"\n");
    return 1;
  }

  return 0;
}

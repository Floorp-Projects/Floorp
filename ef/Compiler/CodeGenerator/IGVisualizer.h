/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/*
	The instruction graph visulizer.
	IGVisualizer.h

	Peter DeSantis
*/

#ifndef IGVISUALIZER
#define IGVISUALIZER
#if defined(DEBUG) && (defined(WIN32) || defined(USE_MESA)) && defined(IGVISUALIZE)

#include "CodeGenerator.h"
class Instruction;
class Vector;

struct TreeLocation
{
  float rootMiddle;
  int right;
  int top;
  int bottom;
};

struct VisualInstructionRoot
{
	Instruction* root;
	bool exploringAsRoot;
	Vector<TreeLocation>* locAsCSE;
	TreeLocation* currentCSE;
	TreeLocation* locAsRoot;
};

class IGVisualizer
{

private:
  void *font;
  float zoomFactor;
  double cumZoom;
  double centerX, centerY;
  double objectCenterX, objectCenterY;
  double mouseCenterX, mouseCenterY;
  double xDimGL, yDimGL;
  double nodeLocX, nodeLocY;


  int xWindowDim, yWindowDim; 
  
  int INSTRUCTIONWIDTH, INSTRUCTIONHEIGHT; 
  int FRAMEWIDTH, FRAMEHEIGHT;
  
  int SPACEBETWEENTREES;
  int BORDERWIDTH;
  uint ARGUMENTREGISTERS;  
  static IGVisualizer* self;
 
  Vector<VisualInstructionRoot>* roots;

  static void visualizeRoots();
  static TreeLocation& DrawTree(Instruction& root, int yBottom, int xLeft);
  static void DrawNode(double xMiddle, int yBottom, Instruction* inst, Vector<TreeLocation>& subTrees);
  static void DrawDummyNode(double xMiddle, int yBottom);
  static void DrawArgumentNode(double xMiddle, int yBottom);
  static VisualInstructionRoot* isInRoots(Instruction* root);
  static void DrawSubExpressionNode(Instruction* inst, double xMIddle, double yBottom);
  static void LabelNode(Instruction* inst);
  static VisualInstructionRoot* findRootAtLoc(double x, double y);
  static bool isInNode(double x, double y, double middle, int bottom);

public:
  static void selectZoom(int msg);
  static void selectRoot(int msg);
  static void mainMenu(int msg);
  static void mouse(int button, int state, int x, int y);
  static void keyboard(unsigned char key, int x, int y);
  static void reshape(int width, int height);
  static void tick(void);
  static void display();
  
  void addRoots(Vector<RootPair>& roots);
  void visualize();
  IGVisualizer();
  ~IGVisualizer();

};


#endif //#if defined(DEBUG) && defined(WIN32) && defined(IGVISUALIZE)
#endif // IGVISUALIZER

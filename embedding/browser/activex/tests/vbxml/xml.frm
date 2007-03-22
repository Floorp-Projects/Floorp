VERSION 5.00
Object = "{831FDD16-0C5C-11D2-A9FC-0000F8754DA1}#2.0#0"; "MSCOMCTL.OCX"
Begin VB.Form Form1 
   Caption         =   "Form1"
   ClientHeight    =   4932
   ClientLeft      =   48
   ClientTop       =   276
   ClientWidth     =   5052
   LinkTopic       =   "Form1"
   ScaleHeight     =   4932
   ScaleWidth      =   5052
   StartUpPosition =   3  'Windows Default
   Begin MSComctlLib.TreeView treeXML 
      Height          =   4692
      Left            =   120
      TabIndex        =   3
      Top             =   120
      Width           =   3132
      _ExtentX        =   5525
      _ExtentY        =   8276
      _Version        =   393217
      Indentation     =   176
      LineStyle       =   1
      Style           =   7
      Appearance      =   1
   End
   Begin VB.TextBox txtURL 
      Height          =   288
      Left            =   3480
      TabIndex        =   1
      Text            =   "M:\moz\mozilla\webshell\embed\ActiveX\xml\tests\vbxml\test.xml"
      Top             =   480
      Width           =   1332
   End
   Begin VB.CommandButton Command1 
      Caption         =   "Parse XML"
      Height          =   492
      Left            =   3480
      TabIndex        =   0
      Top             =   960
      Width           =   1332
   End
   Begin VB.Label Label1 
      Caption         =   "XML File:"
      Height          =   252
      Left            =   3480
      TabIndex        =   2
      Top             =   120
      Width           =   852
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Private Sub Command1_Click()
    Dim o As New MozXMLDocument
    o.URL = txtURL.Text
    Debug.Print o.root.tagName
    treeXML.Nodes.Add , , "R", "XML"
    buildBranch "R", o.root
End Sub

Sub buildBranch(ByRef sParentKey As String, o As Object)
    Dim n As Integer
    Dim c As Object
    Dim sKey As String
    
    Set c = o.children
    For i = 0 To c.length - 1
        sKey = sParentKey + "." + CStr(i)
        treeXML.Nodes.Add sParentKey, tvwChild, sKey, c.Item(i).tagName
        buildBranch sKey, c.Item(i)
    Next
End Sub

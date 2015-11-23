#include <vector>
#include "mozilla/RefPtr.h"
#include "gtest/gtest.h"
#include "TreeTraversal.h"

using namespace mozilla::layers;
using namespace mozilla;

enum class SearchNodeType {Needle, Hay};
enum class ForEachNodeType {Continue, Skip};

template <class T>
class TestNode {
  public:
    NS_INLINE_DECL_REFCOUNTING(TestNode<T>);
    explicit TestNode(T aType, int aExpectedTraversalRank = -1);
    TestNode<T>* GetLastChild();
    TestNode<T>* GetPrevSibling();
    void SetPrevSibling(RefPtr<TestNode<T>> aNode);
    void AddChild(RefPtr<TestNode<T>> aNode);
    void SetActualTraversalRank(int aRank);
    int GetExpectedTraversalRank();
    int GetActualTraversalRank();
    T GetType();
  private:
    RefPtr<TestNode<T>> mPreviousNode;
    RefPtr<TestNode<T>> mLastChildNode;
    int mExpectedTraversalRank;
    int mActualTraversalRank;
    T mType;
    ~TestNode<T>() {};
};

template <class T>
TestNode<T>::TestNode(T aType, int aExpectedTraversalRank) : 
  mExpectedTraversalRank(aExpectedTraversalRank),
  mActualTraversalRank(-1),
  mType(aType)
{
}

template <class T>
void TestNode<T>::AddChild(RefPtr<TestNode<T>> aNode)
{
  aNode->SetPrevSibling(mLastChildNode);
  mLastChildNode = aNode;
}

template <class T>
void TestNode<T>::SetPrevSibling(RefPtr<TestNode<T>> aNode)
{
  mPreviousNode = aNode;
}

template <class T>
TestNode<T>* TestNode<T>::GetLastChild()
{
  return mLastChildNode;
}

template <class T>
TestNode<T>* TestNode<T>::GetPrevSibling()
{
  return mPreviousNode;
}

template <class T>
int TestNode<T>::GetActualTraversalRank()
{
  return mActualTraversalRank;
}

template <class T>
void TestNode<T>::SetActualTraversalRank(int aRank)
{
  mActualTraversalRank = aRank;
}

template <class T>
int TestNode<T>::GetExpectedTraversalRank()
{
  return mExpectedTraversalRank;
}

template <class T>
T TestNode<T>::GetType()
{
  return mType;
}

typedef TestNode<SearchNodeType> SearchTestNode;
typedef TestNode<ForEachNodeType> ForEachTestNode;

TEST(TreeTraversal, DepthFirstSearchNull)
{
  RefPtr<SearchTestNode> nullNode;
  RefPtr<SearchTestNode> result = DepthFirstSearch(nullNode.get(),
      [](SearchTestNode* aNode)
      {
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result.get(), nullptr) << "Null root did not return null search result.";
}

TEST(TreeTraversal, DepthFirstSearchValueExists)
{
  int visitCount = 0;
  size_t expectedNeedleTraversalRank = 7;
  RefPtr<SearchTestNode> needleNode;
  std::vector<RefPtr<SearchTestNode>> nodeList;
  for (size_t i = 0; i < 10; i++)
  {
    if (i == expectedNeedleTraversalRank) {
      needleNode = new SearchTestNode(SearchNodeType::Needle, i);
      nodeList.push_back(needleNode);
    } else if (i < expectedNeedleTraversalRank) {
      nodeList.push_back(new SearchTestNode(SearchNodeType::Hay, i));
    } else {
      nodeList.push_back(new SearchTestNode(SearchNodeType::Hay));
    }
  }

  RefPtr<SearchTestNode> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);

  RefPtr<SearchTestNode> foundNode = DepthFirstSearch(root.get(),
      [&visitCount](SearchTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode, needleNode) << "Search did not return expected node.";
  ASSERT_EQ(foundNode->GetType(), SearchNodeType::Needle)
      << "Returned node does not match expected value (something odd happened).";
}

TEST(TreeTraversal, DepthFirstSearchRootIsNeedle)
{
  RefPtr<SearchTestNode> root = new SearchTestNode(SearchNodeType::Needle, 0);
  RefPtr<SearchTestNode> childNode1= new SearchTestNode(SearchNodeType::Hay);
  RefPtr<SearchTestNode> childNode2 = new SearchTestNode(SearchNodeType::Hay);
  int visitCount = 0;
  RefPtr<SearchTestNode> result = DepthFirstSearch(root.get(),
      [&visitCount](SearchTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result, root) << "Search starting at needle did not return needle.";
  ASSERT_EQ(root->GetExpectedTraversalRank(), root->GetActualTraversalRank())
      << "Search starting at needle did not return needle.";
  ASSERT_EQ(childNode1->GetExpectedTraversalRank(),
      childNode1->GetActualTraversalRank()) 
      << "Search starting at needle continued past needle.";
  ASSERT_EQ(childNode2->GetExpectedTraversalRank(),
      childNode2->GetActualTraversalRank()) 
      << "Search starting at needle continued past needle.";
}

TEST(TreeTraversal, DepthFirstSearchValueDoesNotExist)
{
  int visitCount = 0;
  std::vector<RefPtr<SearchTestNode>> nodeList;
  for (int i = 0; i < 10; i++)
  {
      nodeList.push_back(new SearchTestNode(SearchNodeType::Hay, i));
  }

  RefPtr<SearchTestNode> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);


  RefPtr<SearchTestNode> foundNode = DepthFirstSearch(root.get(),
      [&visitCount](SearchTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
	visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (int i = 0; i < 10; i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(), 
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode.get(), nullptr) 
      << "Search found something that should not exist.";
}

TEST(TreeTraversal, BreadthFirstSearchNull)
{
  RefPtr<SearchTestNode> nullNode;
  RefPtr<SearchTestNode> result = BreadthFirstSearch(nullNode.get(),
      [](SearchTestNode* aNode)
      {
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result.get(), nullptr) << "Null root did not return null search result.";
}

TEST(TreeTraversal, BreadthFirstSearchRootIsNeedle)
{
  RefPtr<SearchTestNode> root = new SearchTestNode(SearchNodeType::Needle, 0);
  RefPtr<SearchTestNode> childNode1= new SearchTestNode(SearchNodeType::Hay);
  RefPtr<SearchTestNode> childNode2 = new SearchTestNode(SearchNodeType::Hay);
  int visitCount = 0;
  RefPtr<SearchTestNode> result = BreadthFirstSearch(root.get(),
      [&visitCount](SearchTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result, root) << "Search starting at needle did not return needle.";
  ASSERT_EQ(root->GetExpectedTraversalRank(), root->GetActualTraversalRank())
      << "Search starting at needle did not return needle.";
  ASSERT_EQ(childNode1->GetExpectedTraversalRank(),
      childNode1->GetActualTraversalRank()) 
      << "Search starting at needle continued past needle.";
  ASSERT_EQ(childNode2->GetExpectedTraversalRank(),
      childNode2->GetActualTraversalRank()) 
      << "Search starting at needle continued past needle.";
}

TEST(TreeTraversal, BreadthFirstSearchValueExists)
{
  int visitCount = 0;
  size_t expectedNeedleTraversalRank = 7;
  RefPtr<SearchTestNode> needleNode;
  std::vector<RefPtr<SearchTestNode>> nodeList;
  for (size_t i = 0; i < 10; i++)
  {
    if (i == expectedNeedleTraversalRank) {
      needleNode = new SearchTestNode(SearchNodeType::Needle, i);
      nodeList.push_back(needleNode);
    } else if (i < expectedNeedleTraversalRank) {
      nodeList.push_back(new SearchTestNode(SearchNodeType::Hay, i));
    } else {
      nodeList.push_back(new SearchTestNode(SearchNodeType::Hay));
    }
  }

  RefPtr<SearchTestNode> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[2]);
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[1]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[2]->AddChild(nodeList[6]);
  nodeList[2]->AddChild(nodeList[5]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[9]);
  nodeList[7]->AddChild(nodeList[8]);

  RefPtr<SearchTestNode> foundNode = BreadthFirstSearch(root.get(),
      [&visitCount](SearchTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
	visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode, needleNode) << "Search did not return expected node.";
  ASSERT_EQ(foundNode->GetType(), SearchNodeType::Needle)
      << "Returned node does not match expected value (something odd happened).";
}

TEST(TreeTraversal, BreadthFirstSearchValueDoesNotExist)
{
  int visitCount = 0;
  std::vector<RefPtr<SearchTestNode>> nodeList;
  for (int i = 0; i < 10; i++)
  {
    nodeList.push_back(new SearchTestNode(SearchNodeType::Hay, i));
  }

  RefPtr<SearchTestNode> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[2]);
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[1]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[2]->AddChild(nodeList[6]);
  nodeList[2]->AddChild(nodeList[5]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[9]);
  nodeList[7]->AddChild(nodeList[8]);


  RefPtr<SearchTestNode> foundNode = BreadthFirstSearch(root.get(),
      [&visitCount](SearchTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
	visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
      ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(), 
          nodeList[i]->GetActualTraversalRank())
          << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode.get(), nullptr) 
      << "Search found something that should not exist.";
}

TEST(TreeTraversal, ForEachNodeNullStillRuns)
{
  RefPtr<ForEachTestNode> nullNode;
  ForEachNode(nullNode.get(),
    [](ForEachTestNode* aNode)
    {
      return TraversalFlag::Continue;
    });
}

TEST(TreeTraversal, ForEachNodeAllEligible)
{
  std::vector<RefPtr<ForEachTestNode>> nodeList;
  int visitCount = 0;
  for (int i = 0; i < 10; i++)
  {
    nodeList.push_back(new ForEachTestNode(ForEachNodeType::Continue,i));
  }

  RefPtr<ForEachTestNode> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);


  ForEachNode(root.get(),
      [&visitCount](ForEachTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
	visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
	    ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(), 
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }
}

TEST(TreeTraversal, ForEachNodeSomeIneligibleNodes)
{
  std::vector<RefPtr<ForEachTestNode>> expectedVisitedNodeList;
  std::vector<RefPtr<ForEachTestNode>> expectedSkippedNodeList;
  int visitCount = 0;
  
  expectedVisitedNodeList.push_back(new ForEachTestNode(ForEachNodeType::Continue, 0));
  expectedVisitedNodeList.push_back(new ForEachTestNode(ForEachNodeType::Skip, 1));
  expectedVisitedNodeList.push_back(new ForEachTestNode(ForEachNodeType::Continue, 2));
  expectedVisitedNodeList.push_back(new ForEachTestNode(ForEachNodeType::Skip, 3));

  expectedSkippedNodeList.push_back(new ForEachTestNode(ForEachNodeType::Continue));
  expectedSkippedNodeList.push_back(new ForEachTestNode(ForEachNodeType::Continue));
  expectedSkippedNodeList.push_back(new ForEachTestNode(ForEachNodeType::Skip));
  expectedSkippedNodeList.push_back(new ForEachTestNode(ForEachNodeType::Skip));

  RefPtr<ForEachTestNode> root = expectedVisitedNodeList[0];
  expectedVisitedNodeList[0]->AddChild(expectedVisitedNodeList[1]);
  expectedVisitedNodeList[0]->AddChild(expectedVisitedNodeList[2]);
  expectedVisitedNodeList[1]->AddChild(expectedSkippedNodeList[0]);
  expectedVisitedNodeList[1]->AddChild(expectedSkippedNodeList[1]);
  expectedVisitedNodeList[2]->AddChild(expectedVisitedNodeList[3]);
  expectedVisitedNodeList[3]->AddChild(expectedSkippedNodeList[2]);
  expectedVisitedNodeList[3]->AddChild(expectedSkippedNodeList[3]);

  ForEachNode(root.get(),
      [&visitCount](ForEachTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
	visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
	    ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  for (size_t i = 0; i < expectedVisitedNodeList.size(); i++)
  {
    ASSERT_EQ(expectedVisitedNodeList[i]->GetExpectedTraversalRank(), 
        expectedVisitedNodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }
  
  for (size_t i = 0; i < expectedSkippedNodeList.size(); i++)
  { 
    ASSERT_EQ(expectedSkippedNodeList[i]->GetExpectedTraversalRank(),
        expectedSkippedNodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << "was not expected to be hit.";
  }
}

TEST(TreeTraversal, ForEachNodeIneligibleRoot)
{
  int visitCount = 0;

  RefPtr<ForEachTestNode> root = new ForEachTestNode(ForEachNodeType::Skip, 0);
  RefPtr<ForEachTestNode> childNode1 = new ForEachTestNode(ForEachNodeType::Continue);
  RefPtr<ForEachTestNode> chlidNode2 = new ForEachTestNode(ForEachNodeType::Skip);

  ForEachNode(root.get(),
      [&visitCount](ForEachTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
	visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
	    ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  ASSERT_EQ(root->GetExpectedTraversalRank(), root->GetActualTraversalRank())
      << "Root was hit out of order.";
  ASSERT_EQ(childNode1->GetExpectedTraversalRank(), childNode1->GetActualTraversalRank())
      << "Eligible child was still hit.";
  ASSERT_EQ(chlidNode2->GetExpectedTraversalRank(), chlidNode2->GetActualTraversalRank())
      << "Ineligible child was still hit.";
}

TEST(TreeTraversal, ForEachNodeLeavesIneligible)
{

  std::vector<RefPtr<ForEachTestNode>> nodeList;
  int visitCount = 0;
  for (int i = 0; i < 10; i++)
  {
    if (i == 1 || i == 9) {
      nodeList.push_back(new ForEachTestNode(ForEachNodeType::Skip, i));
    } else {
      nodeList.push_back(new ForEachTestNode(ForEachNodeType::Continue, i));
    }
  }

  RefPtr<ForEachTestNode> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[2]);
  nodeList[2]->AddChild(nodeList[3]);
  nodeList[2]->AddChild(nodeList[4]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);

  ForEachNode(root.get(),
      [&visitCount](ForEachTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
	visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
	    ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(), 
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }
}

TEST(TreeTraversal, ForEachNodeLambdaReturnsVoid)
{
  std::vector<RefPtr<ForEachTestNode>> nodeList;
  int visitCount = 0;
  for (int i = 0; i < 10; i++)
  {
    nodeList.push_back(new ForEachTestNode(ForEachNodeType::Continue,i));
  }

  RefPtr<ForEachTestNode> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);


  ForEachNode(root.get(),
      [&visitCount](ForEachTestNode* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
	visitCount++;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }
}
